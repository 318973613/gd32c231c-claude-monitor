#include "app_serial_clock.h"
#include "app_ai_link.h"
#include "gd32c2x1.h"
#include <string.h>

#define RX_LINE_SIZE              96U
#define USART_BAUDRATE            115200U

static volatile char rx_line[RX_LINE_SIZE];
static volatile uint8_t rx_index = 0U;
static volatile uint8_t rx_ready = 0U;
static volatile uint32_t rx_count = 0U;
static volatile uint32_t rx_overflow_count = 0U;

static app_clock_state_t clock_state = {0U, 0U, 0U, 0U, 0U};
static TickType_t clock_last_tick = 0U;

static void serial_write_char(char ch);
static void serial_write_string(const char *text);
static uint8_t parse_time_command(const char *line, uint32_t *seconds);
static uint8_t parse_two_digits(const char *text, uint8_t *value);
static uint8_t starts_with(const char *text, const char *prefix);

void app_serial_clock_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, GPIO_PIN_9 | GPIO_PIN_10);

    usart_deinit(USART0);
    usart_baudrate_set(USART0, USART_BAUDRATE);
    usart_word_length_set(USART0, USART_WL_8BIT);
    usart_stop_bit_set(USART0, USART_STB_1BIT);
    usart_parity_config(USART0, USART_PM_NONE);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_enable(USART0);

    nvic_irq_enable(USART0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    usart_interrupt_enable(USART0, USART_INT_RBNE);

    serial_write_string("\r\nClaude UI dashboard ready.\r\n");
    serial_write_string("Set clock with TIME=HH:MM:SS or T=HH:MM:SS\r\n");
}

void app_serial_clock_write(const char *text)
{
    if(NULL != text) {
        serial_write_string(text);
    }
}

void app_serial_clock_update(TickType_t now)
{
    TickType_t elapsed_ticks;
    uint32_t elapsed_seconds;

    if(0U == clock_state.valid) {
        clock_last_tick = now;
        return;
    }

    elapsed_ticks = now - clock_last_tick;
    elapsed_seconds = (uint32_t)(elapsed_ticks / pdMS_TO_TICKS(1000U));
    if(0U == elapsed_seconds) {
        return;
    }

    clock_last_tick += (TickType_t)(elapsed_seconds * pdMS_TO_TICKS(1000U));
    clock_state.seconds = (clock_state.seconds + elapsed_seconds) % 86400U;
}

uint8_t app_serial_clock_process(void)
{
    char line[RX_LINE_SIZE];
    const char *time_text;
    uint32_t seconds;
    uint8_t has_line;

    taskENTER_CRITICAL();
    has_line = rx_ready;
    if(0U != has_line) {
        (void)memcpy(line, (const void *)rx_line, RX_LINE_SIZE);
        rx_ready = 0U;
    }
    taskEXIT_CRITICAL();

    if(0U == has_line) {
        return 0U;
    }

    clock_state.rx_count++;

    if(0U != parse_time_command(line, &seconds)) {
        time_text = (0U != starts_with(line, "TIME=")) ? (line + 5) : (line + 2);
        clock_state.seconds = seconds;
        clock_last_tick = xTaskGetTickCount();
        clock_state.valid = 1U;
        clock_state.set_count++;
        serial_write_string("\r\nOK TIME=");
        serial_write_string(time_text);
        serial_write_string("\r\n");
    } else if(0U != app_ai_link_process_line(line)) {
        /* AI status command accepted by app_ai_link. */
    } else {
        serial_write_string("\r\nERR use TIME=HH:MM:SS\r\n");
    }

    return 1U;
}

void app_serial_clock_get_state(app_clock_state_t *state)
{
    if(NULL == state) {
        return;
    }

    taskENTER_CRITICAL();
    *state = clock_state;
    state->overflow_count = rx_overflow_count;
    taskEXIT_CRITICAL();
}

void USART0_IRQHandler(void)
{
    char ch;

    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_RBNE)) {
        ch = (char)usart_data_receive(USART0);

        if((ch >= 'a') && (ch <= 'z')) {
            ch = (char)(ch - 'a' + 'A');
        }

        if(('\r' == ch) || ('\n' == ch)) {
            if((rx_index > 0U) && (0U == rx_ready)) {
                rx_line[rx_index] = '\0';
                rx_index = 0U;
                rx_ready = 1U;
                rx_count++;
            } else {
                rx_index = 0U;
            }
        } else if(rx_index < (RX_LINE_SIZE - 1U)) {
            rx_line[rx_index] = ch;
            rx_index++;
        } else {
            rx_index = 0U;
            rx_overflow_count++;
        }
    }
}

static void serial_write_char(char ch)
{
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE)) {
    }
    usart_data_transmit(USART0, (uint16_t)ch);
}

static void serial_write_string(const char *text)
{
    while('\0' != *text) {
        serial_write_char(*text);
        text++;
    }
}

static uint8_t parse_time_command(const char *line, uint32_t *seconds)
{
    const char *time_text;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    if(0U != starts_with(line, "TIME=")) {
        time_text = line + 5;
    } else if(0U != starts_with(line, "T=")) {
        time_text = line + 2;
    } else {
        return 0U;
    }

    if((0U == parse_two_digits(&time_text[0], &hour)) ||
       (':' != time_text[2]) ||
       (0U == parse_two_digits(&time_text[3], &minute)) ||
       (':' != time_text[5]) ||
       (0U == parse_two_digits(&time_text[6], &second)) ||
       ('\0' != time_text[8])) {
        return 0U;
    }

    if((hour > 23U) || (minute > 59U) || (second > 59U)) {
        return 0U;
    }

    *seconds = ((uint32_t)hour * 3600U) + ((uint32_t)minute * 60U) + second;
    return 1U;
}

static uint8_t parse_two_digits(const char *text, uint8_t *value)
{
    if((text[0] < '0') || (text[0] > '9') || (text[1] < '0') || (text[1] > '9')) {
        return 0U;
    }

    *value = (uint8_t)(((uint8_t)(text[0] - '0') * 10U) + (uint8_t)(text[1] - '0'));
    return 1U;
}

static uint8_t starts_with(const char *text, const char *prefix)
{
    while('\0' != *prefix) {
        if(*text != *prefix) {
            return 0U;
        }
        text++;
        prefix++;
    }

    return 1U;
}
