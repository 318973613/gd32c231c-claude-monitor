/*!
    \file    main.c
    \brief   FreeRTOS USART status link demo for GD32C231C-EVAL

    \version 2026-06-17, custom FreeRTOS demo for gd32c2x1
*/

#include "gd32c2x1.h"
#include "gd32c231c_eval.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LED_COUNT                 4U
#define RX_LINE_SIZE              64U
#define RX_QUEUE_LENGTH           96U
#define HEARTBEAT_PERIOD_MS       500U
#define RX_PULSE_MS               120U
#define MONITOR_PERIOD_MS         5000U

#define LED_TASK_STACK_WORDS      160U
#define SERIAL_TASK_STACK_WORDS   320U
#define MONITOR_TASK_STACK_WORDS  220U

#define LED_TASK_PRIORITY         (tskIDLE_PRIORITY + 1U)
#define SERIAL_TASK_PRIORITY      (tskIDLE_PRIORITY + 2U)
#define MONITOR_TASK_PRIORITY     (tskIDLE_PRIORITY + 1U)

static const uint32_t led_port[LED_COUNT] = {
    GPIOA,
    GPIOD,
    GPIOD,
    GPIOB
};

static const uint32_t led_pin[LED_COUNT] = {
    GPIO_PIN_15,
    GPIO_PIN_1,
    GPIO_PIN_3,
    GPIO_PIN_4
};

static QueueHandle_t rx_queue;
static SemaphoreHandle_t print_mutex;
static volatile uint8_t heartbeat_on = 0U;
static volatile uint8_t state_running = 0U;
static volatile uint8_t state_error = 0U;
static volatile TickType_t rx_pulse_until = 0U;

static void led_gpio_config(void);
static void led_write(uint32_t index, uint8_t on);
static void led_all(uint8_t on);
static void serial_irq_config(void);
static void led_task(void *argument);
static void serial_task(void *argument);
static void monitor_task(void *argument);
static void process_command(uint8_t *cmd, uint32_t len);
static void print_rx_trace(const uint8_t *cmd, uint32_t len);
static void print_help(void);
static void serial_printf(const char *format, ...);
static uint8_t command_equals(const uint8_t *cmd, const char *text);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    led_gpio_config();
    gd_eval_com_init(EVAL_COM);

    rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(uint8_t));
    print_mutex = xSemaphoreCreateMutex();

    if((NULL == rx_queue) || (NULL == print_mutex)) {
        led_all(1U);
        while(1) {
        }
    }

    printf("\r\n[GD32C231C] FreeRTOS status link ready.\r\n");
    printf("USART0: PA9(TX), PA10(RX), 115200 8N1\r\n");
    print_help();

    if(pdPASS != xTaskCreate(led_task, "led", LED_TASK_STACK_WORDS, NULL, LED_TASK_PRIORITY, NULL)) {
        led_all(1U);
        while(1) {
        }
    }
    if(pdPASS != xTaskCreate(serial_task, "serial", SERIAL_TASK_STACK_WORDS, NULL, SERIAL_TASK_PRIORITY, NULL)) {
        led_all(1U);
        while(1) {
        }
    }
    if(pdPASS != xTaskCreate(monitor_task, "monitor", MONITOR_TASK_STACK_WORDS, NULL, MONITOR_TASK_PRIORITY, NULL)) {
        led_all(1U);
        while(1) {
        }
    }

    serial_irq_config();
    vTaskStartScheduler();

    led_all(1U);
    while(1) {
    }
}

/*!
    \brief      configure all four board LEDs
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void led_gpio_config(void)
{
    uint32_t i;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOB);

    for(i = 0U; i < LED_COUNT; i++) {
        gpio_mode_set(led_port[i], GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, led_pin[i]);
        gpio_output_options_set(led_port[i], GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, led_pin[i]);
    }

    led_all(0U);
}

/*!
    \brief      set one LED
    \param[in]  index: 0 LED1, 1 LED2, 2 LED3, 3 LED4
    \param[in]  on: 0 off, non-zero on
    \param[out] none
    \retval     none
*/
static void led_write(uint32_t index, uint8_t on)
{
    if(index >= LED_COUNT) {
        return;
    }

    if(0U != on) {
        gpio_bit_set(led_port[index], led_pin[index]);
    } else {
        gpio_bit_reset(led_port[index], led_pin[index]);
    }
}

/*!
    \brief      set all LEDs
    \param[in]  on: 0 off, non-zero on
    \param[out] none
    \retval     none
*/
static void led_all(uint8_t on)
{
    uint32_t i;

    for(i = 0U; i < LED_COUNT; i++) {
        led_write(i, on);
    }
}

/*!
    \brief      enable USART0 receive interrupt
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void serial_irq_config(void)
{
    nvic_irq_enable(USART0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    usart_interrupt_enable(EVAL_COM, USART_INT_RBNE);
}

/*!
    \brief      LED task: heartbeat and command status indicators
    \param[in]  argument: unused
    \param[out] none
    \retval     none
*/
static void led_task(void *argument)
{
    TickType_t last_wake;
    TickType_t last_heartbeat;
    TickType_t now;

    (void)argument;
    last_wake = xTaskGetTickCount();
    last_heartbeat = last_wake;

    while(1) {
        now = xTaskGetTickCount();
        if((now - last_heartbeat) >= pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS)) {
            last_heartbeat = now;
            heartbeat_on = (uint8_t)!heartbeat_on;
        }

        led_write(0U, heartbeat_on);
        led_write(1U, state_running);
        led_write(2U, (uint8_t)(now < rx_pulse_until));
        led_write(3U, state_error);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(50U));
    }
}

/*!
    \brief      serial task: build command lines from ISR-fed byte queue
    \param[in]  argument: unused
    \param[out] none
    \retval     none
*/
static void serial_task(void *argument)
{
    uint8_t ch;
    uint8_t line[RX_LINE_SIZE];
    uint32_t len = 0U;

    (void)argument;

    while(1) {
        if(pdPASS == xQueueReceive(rx_queue, &ch, portMAX_DELAY)) {
            if((ch == '\r') || (ch == '\n')) {
                if(len > 0U) {
                    line[len] = '\0';
                    process_command(line, len);
                    len = 0U;
                }
            } else if(len < (RX_LINE_SIZE - 1U)) {
                if((ch >= 'a') && (ch <= 'z')) {
                    ch = (uint8_t)(ch - 'a' + 'A');
                }
                line[len++] = ch;
            } else {
                len = 0U;
                state_error = 1U;
                serial_printf("\r\nERR line too long\r\n");
            }
        }
    }
}

/*!
    \brief      periodic runtime heartbeat over USART
    \param[in]  argument: unused
    \param[out] none
    \retval     none
*/
static void monitor_task(void *argument)
{
    (void)argument;

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(MONITOR_PERIOD_MS));
        serial_printf("\r\nRTOS tick=%lu queue=%lu heap=%lu\r\n",
                      (unsigned long)xTaskGetTickCount(),
                      (unsigned long)uxQueueMessagesWaiting(rx_queue),
                      (unsigned long)xPortGetFreeHeapSize());
    }
}

/*!
    \brief      process one received command
    \param[in]  cmd: zero-terminated uppercase command
    \param[in]  len: command length
    \param[out] none
    \retval     none
*/
static void process_command(uint8_t *cmd, uint32_t len)
{
    rx_pulse_until = xTaskGetTickCount() + pdMS_TO_TICKS(RX_PULSE_MS);
    print_rx_trace(cmd, len);

    if(command_equals(cmd, "PING") || command_equals(cmd, "P")) {
        serial_printf("\r\nPONG GD32C231C FreeRTOS OK\r\n");
    } else if(command_equals(cmd, "IDLE") || command_equals(cmd, "I")) {
        state_running = 0U;
        state_error = 0U;
        serial_printf("\r\nACK STATE=IDLE\r\n");
    } else if(command_equals(cmd, "RUN") || command_equals(cmd, "R")) {
        state_running = 1U;
        state_error = 0U;
        serial_printf("\r\nACK STATE=RUNNING\r\n");
    } else if(command_equals(cmd, "DONE") || command_equals(cmd, "D")) {
        state_running = 0U;
        state_error = 0U;
        serial_printf("\r\nACK STATE=DONE\r\n");
        led_all(1U);
        vTaskDelay(pdMS_TO_TICKS(120U));
        led_all(0U);
    } else if(command_equals(cmd, "ERR") || command_equals(cmd, "E")) {
        state_running = 0U;
        state_error = 1U;
        serial_printf("\r\nACK STATE=ERROR\r\n");
    } else if(command_equals(cmd, "HELP") || command_equals(cmd, "H")) {
        print_help();
    } else {
        state_error = 1U;
        serial_printf("\r\nERR unknown command: %s\r\n", cmd);
        print_help();
    }
}

/*!
    \brief      print received command with ASCII and HEX bytes
    \param[in]  cmd: command buffer
    \param[in]  len: command length
    \param[out] none
    \retval     none
*/
static void print_rx_trace(const uint8_t *cmd, uint32_t len)
{
    uint32_t i;

    serial_printf("\r\nRX[%lu]: %s\r\nHEX:", (unsigned long)len, cmd);
    for(i = 0U; i < len; i++) {
        serial_printf(" %02X", cmd[i]);
    }
    serial_printf("\r\n");
}

/*!
    \brief      print command help
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void print_help(void)
{
    serial_printf("\r\nCommands: PING/P, IDLE/I, RUN/R, DONE/D, ERR/E, HELP/H\r\n");
    serial_printf("FreeRTOS tasks: LED, SERIAL, MONITOR. LED1 heartbeat, LED2 running, LED3 rx pulse, LED4 error.\r\n");
}

/*!
    \brief      printf protected by a FreeRTOS mutex after scheduler start
    \param[in]  format: printf format string
    \param[out] none
    \retval     none
*/
static void serial_printf(const char *format, ...)
{
    va_list args;

    if((NULL != print_mutex) && (taskSCHEDULER_RUNNING == xTaskGetSchedulerState())) {
        (void)xSemaphoreTake(print_mutex, portMAX_DELAY);
    }

    va_start(args, format);
    (void)vprintf(format, args);
    va_end(args);

    if((NULL != print_mutex) && (taskSCHEDULER_RUNNING == xTaskGetSchedulerState())) {
        (void)xSemaphoreGive(print_mutex);
    }
}

/*!
    \brief      compare command with constant text
    \param[in]  cmd: command buffer
    \param[in]  text: expected text
    \param[out] none
    \retval     1 if equal, otherwise 0
*/
static uint8_t command_equals(const uint8_t *cmd, const char *text)
{
    return (0 == strcmp((const char *)cmd, text)) ? 1U : 0U;
}

/*!
    \brief      USART0 interrupt handler, push received bytes into FreeRTOS queue
    \param[in]  none
    \param[out] none
    \retval     none
*/
void USART0_IRQHandler(void)
{
    uint8_t ch;
    BaseType_t higher_priority_task_woken = pdFALSE;

    if(RESET != usart_interrupt_flag_get(EVAL_COM, USART_INT_FLAG_RBNE)) {
        ch = (uint8_t)usart_data_receive(EVAL_COM);
        if(NULL != rx_queue) {
            (void)xQueueSendFromISR(rx_queue, &ch, &higher_priority_task_woken);
            portYIELD_FROM_ISR(higher_priority_task_woken);
        }
    }
}

void vApplicationMallocFailedHook(void)
{
    state_error = 1U;
    led_all(1U);
    taskDISABLE_INTERRUPTS();
    while(1) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    state_error = 1U;
    led_all(1U);
    taskDISABLE_INTERRUPTS();
    while(1) {
    }
}

void vApplicationIdleHook(void)
{
}
