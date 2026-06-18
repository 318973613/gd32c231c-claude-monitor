#include "app_ai_link.h"
#include "app_serial_clock.h"
#include "gd32c2x1.h"
#include <string.h>

#define APPROVE_PORT              GPIOA
#define APPROVE_PIN               GPIO_PIN_0
#define DENY_PORT                 GPIOA
#define DENY_PIN                  GPIO_PIN_4
#define BUTTON_SCAN_MS            10U
#define BUTTON_DEBOUNCE_TICKS     1U

static app_ai_state_t ai_state = {
    APP_AI_IDLE, 0U, 0U, 0U, 0U,
    "UNKNOWN", "NONE", 0U, 0U, 0U, 0U, 0U, 0U, 0U
};
static TickType_t last_button_scan = 0U;
static uint8_t approve_last = 0U;
static uint8_t deny_last = 0U;
static uint8_t approve_stable = 0U;
static uint8_t deny_stable = 0U;
static uint8_t approve_reported = 0U;
static uint8_t deny_reported = 0U;

static uint8_t starts_with(const char *text, const char *prefix);
static void set_mode(app_ai_mode_t mode);
static void set_tool(const char *tool);
static void set_model(const char *model);
static uint8_t parse_percent_pair(const char *text, uint8_t *first, uint8_t *second);
static uint8_t parse_uint16(const char *text, uint16_t *value);
static void copy_token(char *dest, uint8_t dest_size, const char *src);
static uint8_t read_pressed(uint32_t port, uint32_t pin);
static uint8_t debounce_button(uint8_t raw, uint8_t *last, uint8_t *stable, uint8_t *reported);

void app_ai_link_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);

    gpio_mode_set(APPROVE_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, APPROVE_PIN);
    gpio_mode_set(DENY_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, DENY_PIN);

    approve_last = read_pressed(APPROVE_PORT, APPROVE_PIN);
    deny_last = read_pressed(DENY_PORT, DENY_PIN);
    approve_stable = 0U;
    deny_stable = 0U;
    approve_reported = 0U;
    deny_reported = 0U;
}

uint8_t app_ai_link_process_line(const char *line)
{
    if(NULL == line) {
        return 0U;
    }

    if(0U != starts_with(line, "AI=IDLE")) {
        set_mode(APP_AI_IDLE);
        set_tool("NONE");
    } else if(0U != starts_with(line, "AI=WORKING")) {
        set_mode(APP_AI_WORKING);
    } else if(0U != starts_with(line, "AI=TOOL:")) {
        set_mode(APP_AI_WORKING);
        set_tool(line + 8);
    } else if(0U != starts_with(line, "AI=NATIVE_APPROVE")) {
        set_mode(APP_AI_NATIVE_APPROVE);
        if(0U != starts_with(line, "AI=NATIVE_APPROVE:")) {
            set_tool(line + 18);
        }
    } else if(0U != starts_with(line, "AI=WAIT_APPROVE")) {
        set_mode(APP_AI_WAIT_APPROVE);
        if(0U != starts_with(line, "AI=WAIT_APPROVE:")) {
            set_tool(line + 16);
        }
    } else if(0U != starts_with(line, "AI=APPROVED")) {
        set_mode(APP_AI_APPROVED);
    } else if(0U != starts_with(line, "AI=DENIED")) {
        set_mode(APP_AI_DENIED);
    } else if(0U != starts_with(line, "AI=DONE")) {
        set_mode(APP_AI_IDLE);
    } else if(0U != starts_with(line, "AI=ERROR")) {
        set_mode(APP_AI_ERROR);
    } else if(0U != starts_with(line, "MODEL=")) {
        set_model(line + 6);
    } else if(0U != starts_with(line, "CTX=")) {
        taskENTER_CRITICAL();
        ai_state.has_context = parse_percent_pair(line + 4,
                                                  &ai_state.context_used_percent,
                                                  &ai_state.context_remaining_percent);
        taskEXIT_CRITICAL();
    } else if(0U != starts_with(line, "RATE=")) {
        taskENTER_CRITICAL();
        ai_state.has_limit = parse_percent_pair(line + 5,
                                                &ai_state.limit_used_percent,
                                                &ai_state.limit_remaining_percent);
        taskEXIT_CRITICAL();
    } else if(0U != starts_with(line, "COST=")) {
        taskENTER_CRITICAL();
        ai_state.has_cost = parse_uint16(line + 5, &ai_state.cost_usd_cents);
        taskEXIT_CRITICAL();
    } else {
        return 0U;
    }

    app_serial_clock_write("\r\nOK AI\r\n");
    return 1U;
}

static void set_tool(const char *tool)
{
    taskENTER_CRITICAL();
    copy_token(ai_state.tool, (uint8_t)sizeof(ai_state.tool), tool);
    taskEXIT_CRITICAL();
}

static void set_model(const char *model)
{
    taskENTER_CRITICAL();
    copy_token(ai_state.model, (uint8_t)sizeof(ai_state.model), model);
    taskEXIT_CRITICAL();
}

static uint8_t parse_percent_pair(const char *text, uint8_t *first, uint8_t *second)
{
    uint16_t a = 0U;
    uint16_t b = 0U;

    if((NULL == text) || (NULL == first) || (NULL == second)) {
        return 0U;
    }

    while((*text >= '0') && (*text <= '9')) {
        a = (uint16_t)((a * 10U) + (uint16_t)(*text - '0'));
        text++;
    }
    if(',' != *text) {
        return 0U;
    }
    text++;
    while((*text >= '0') && (*text <= '9')) {
        b = (uint16_t)((b * 10U) + (uint16_t)(*text - '0'));
        text++;
    }
    if('\0' != *text) {
        return 0U;
    }

    *first = (uint8_t)((a > 100U) ? 100U : a);
    *second = (uint8_t)((b > 100U) ? 100U : b);
    return 1U;
}

static uint8_t parse_uint16(const char *text, uint16_t *value)
{
    uint32_t number = 0U;

    if((NULL == text) || (NULL == value) || ('\0' == *text)) {
        return 0U;
    }

    while((*text >= '0') && (*text <= '9')) {
        number = (number * 10U) + (uint32_t)(*text - '0');
        if(number > 9999U) {
            number = 9999U;
        }
        text++;
    }
    if('\0' != *text) {
        return 0U;
    }

    *value = (uint16_t)number;
    return 1U;
}

static void copy_token(char *dest, uint8_t dest_size, const char *src)
{
    uint8_t i = 0U;
    char ch;

    if((NULL == dest) || (0U == dest_size)) {
        return;
    }
    if(NULL == src) {
        src = "";
    }

    while((i < (uint8_t)(dest_size - 1U)) && ('\0' != *src)) {
        ch = *src;
        if(((ch >= 'A') && (ch <= 'Z')) ||
           ((ch >= '0') && (ch <= '9')) ||
           ('-' == ch) || ('_' == ch) || ('.' == ch)) {
            dest[i] = ch;
            i++;
        }
        src++;
    }
    dest[i] = '\0';
    if(0U == i) {
        dest[0] = '-';
        dest[1] = '\0';
    }
}

uint8_t app_ai_link_update_buttons(TickType_t now)
{
    uint8_t approve_press;
    uint8_t deny_press;

    if((now - last_button_scan) < pdMS_TO_TICKS(BUTTON_SCAN_MS)) {
        return 0U;
    }
    last_button_scan = now;

    approve_press = debounce_button(read_pressed(APPROVE_PORT, APPROVE_PIN),
                                    &approve_last,
                                    &approve_stable,
                                    &approve_reported);
    deny_press = debounce_button(read_pressed(DENY_PORT, DENY_PIN),
                                 &deny_last,
                                 &deny_stable,
                                 &deny_reported);

    if(0U != approve_press) {
        ai_state.approve_count++;
        app_serial_clock_write("BTN=APPROVE\r\n");
        return 1U;
    }
    if(0U != deny_press) {
        ai_state.deny_count++;
        app_serial_clock_write("BTN=DENY\r\n");
        return 1U;
    }

    return 0U;
}

void app_ai_link_get_state(app_ai_state_t *state)
{
    if(NULL == state) {
        return;
    }

    taskENTER_CRITICAL();
    *state = ai_state;
    taskEXIT_CRITICAL();
}

static void set_mode(app_ai_mode_t mode)
{
    taskENTER_CRITICAL();
    if(ai_state.mode != mode) {
        ai_state.mode = mode;
        ai_state.event_count++;
        ai_state.last_change_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    }
    taskEXIT_CRITICAL();
}

static uint8_t read_pressed(uint32_t port, uint32_t pin)
{
    return (RESET != gpio_input_bit_get(port, pin)) ? 1U : 0U;
}

static uint8_t debounce_button(uint8_t raw, uint8_t *last, uint8_t *stable, uint8_t *reported)
{
    uint8_t pressed = 0U;

    if(raw == *last) {
        if(*stable < BUTTON_DEBOUNCE_TICKS) {
            (*stable)++;
        } else if((0U != raw) && (0U == *reported)) {
            pressed = 1U;
            *reported = 1U;
        } else if(0U == raw) {
            *reported = 0U;
        }
    } else {
        *last = raw;
        *stable = 0U;
    }

    return pressed;
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
