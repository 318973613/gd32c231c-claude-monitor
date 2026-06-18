/*!
    \file    main.c
    \brief   USART0 status link demo for GD32C231C-EVAL

    \version 2026-06-17, custom demo for gd32c2x1
*/

#include "gd32c2x1.h"
#include "gd32c231c_eval.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>

#define LED_COUNT             4U
#define RX_LINE_SIZE          64U
#define LOOP_TICK_MS          10U
#define HEARTBEAT_PERIOD_MS   500U
#define RX_PULSE_MS           120U

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

static volatile uint8_t rx_line[RX_LINE_SIZE];
static volatile uint32_t rx_len = 0U;
static volatile uint32_t rx_ready_len = 0U;
static volatile uint8_t rx_ready = 0U;
static volatile uint8_t rx_overflow = 0U;
static uint8_t command_line[RX_LINE_SIZE];
static uint8_t heartbeat_on = 0U;
static uint8_t state_running = 0U;
static uint8_t state_error = 0U;
static uint32_t heartbeat_elapsed = 0U;
static uint32_t rx_pulse_left = 0U;

static void led_gpio_config(void);
static void led_write(uint32_t index, uint8_t on);
static void led_all(uint8_t on);
static void serial_irq_config(void);
static void led_task(void);
static void serial_task(void);
static void process_command(const uint8_t *cmd, uint32_t len);
static void print_rx_trace(const uint8_t *cmd, uint32_t len);
static void print_help(void);
static uint8_t command_equals(const uint8_t *cmd, const char *text);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    systick_config();
    led_gpio_config();
    gd_eval_com_init(EVAL_COM);
    serial_irq_config();

    printf("\r\n[GD32C231C] USART status link ready.\r\n");
    printf("USART0: PA9(TX), PA10(RX), 115200 8N1\r\n");
    print_help();

    while(1) {
        serial_task();
        led_task();
        delay_1ms(LOOP_TICK_MS);
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
    nvic_irq_enable(USART0_IRQn, 0U);
    usart_interrupt_enable(EVAL_COM, USART_INT_RBNE);
}

/*!
    \brief      update LED status indicators
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void led_task(void)
{
    heartbeat_elapsed += LOOP_TICK_MS;
    if(heartbeat_elapsed >= HEARTBEAT_PERIOD_MS) {
        heartbeat_elapsed = 0U;
        heartbeat_on = (uint8_t)!heartbeat_on;
    }

    if(rx_pulse_left > LOOP_TICK_MS) {
        rx_pulse_left -= LOOP_TICK_MS;
    } else {
        rx_pulse_left = 0U;
    }

    led_write(0U, heartbeat_on);                /* LED1: system heartbeat */
    led_write(1U, state_running);               /* LED2: RUN command state */
    led_write(2U, (0U != rx_pulse_left));       /* LED3: RX pulse */
    led_write(3U, state_error);                 /* LED4: error state */
}

/*!
    \brief      handle a command line prepared by USART0 interrupt
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void serial_task(void)
{
    uint32_t i;
    uint32_t len;

    if(0U != rx_overflow) {
        rx_overflow = 0U;
        state_error = 1U;
        printf("\r\nERR line too long\r\n");
    }

    if(0U == rx_ready) {
        return;
    }

    usart_interrupt_disable(EVAL_COM, USART_INT_RBNE);
    len = rx_ready_len;
    if(len >= RX_LINE_SIZE) {
        len = RX_LINE_SIZE - 1U;
    }
    for(i = 0U; i < len; i++) {
        command_line[i] = rx_line[i];
    }
    command_line[len] = '\0';
    rx_ready = 0U;
    rx_ready_len = 0U;
    rx_len = 0U;
    usart_interrupt_enable(EVAL_COM, USART_INT_RBNE);

    process_command(command_line, len);
}

/*!
    \brief      USART0 interrupt handler, receive one command line
    \param[in]  none
    \param[out] none
    \retval     none
*/
void USART0_IRQHandler(void)
{
    uint8_t ch;

    if(RESET != usart_interrupt_flag_get(EVAL_COM, USART_INT_FLAG_RBNE)) {
        ch = (uint8_t)usart_data_receive(EVAL_COM);

        if(0U != rx_ready) {
            return;
        }

        if((ch == '\r') || (ch == '\n')) {
            if(rx_len > 0U) {
                rx_line[rx_len] = '\0';
                rx_ready_len = rx_len;
                rx_ready = 1U;
            }
        } else if(rx_len < (RX_LINE_SIZE - 1U)) {
            if((ch >= 'a') && (ch <= 'z')) {
                ch = (uint8_t)(ch - 'a' + 'A');
            }
            rx_line[rx_len++] = ch;
        } else {
            rx_len = 0U;
            rx_overflow = 1U;
        }
    }
}

/*!
    \brief      process one received command
    \param[in]  cmd: zero-terminated uppercase command
    \param[in]  len: command length
    \param[out] none
    \retval     none
*/
static void process_command(const uint8_t *cmd, uint32_t len)
{
    rx_pulse_left = RX_PULSE_MS;
    print_rx_trace(cmd, len);

    if(command_equals(cmd, "PING") || command_equals(cmd, "P")) {
        printf("\r\nPONG GD32C231C USART OK\r\n");
    } else if(command_equals(cmd, "IDLE") || command_equals(cmd, "I")) {
        state_running = 0U;
        state_error = 0U;
        printf("\r\nACK STATE=IDLE\r\n");
    } else if(command_equals(cmd, "RUN") || command_equals(cmd, "R")) {
        state_running = 1U;
        state_error = 0U;
        printf("\r\nACK STATE=RUNNING\r\n");
    } else if(command_equals(cmd, "DONE") || command_equals(cmd, "D")) {
        state_running = 0U;
        state_error = 0U;
        printf("\r\nACK STATE=DONE\r\n");
        led_all(1U);
        delay_1ms(120U);
        led_all(0U);
    } else if(command_equals(cmd, "ERR") || command_equals(cmd, "E")) {
        state_running = 0U;
        state_error = 1U;
        printf("\r\nACK STATE=ERROR\r\n");
    } else if(command_equals(cmd, "HELP") || command_equals(cmd, "H")) {
        print_help();
    } else {
        state_error = 1U;
        printf("\r\nERR unknown command: %s\r\n", cmd);
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

    printf("\r\nRX[%lu]: %s\r\nHEX:", (unsigned long)len, cmd);
    for(i = 0U; i < len; i++) {
        printf(" %02X", cmd[i]);
    }
    printf("\r\n");
}

/*!
    \brief      print command help
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void print_help(void)
{
    printf("\r\nCommands: PING/P, IDLE/I, RUN/R, DONE/D, ERR/E, HELP/H\r\n");
    printf("LED1 heartbeat, LED2 running, LED3 rx pulse, LED4 error.\r\n");
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
