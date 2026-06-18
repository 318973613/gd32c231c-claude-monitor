#include "app_dashboard.h"
#include "app_flash_assets.h"
#include "app_serial_clock.h"
#include "lcd_driver.h"
#include <string.h>

#define LCD_W                     240U
#define LCD_H                     320U
#define COLOR_BG                  0x0041U
#define COLOR_PANEL               0x08E4U
#define COLOR_PANEL_2             0x1187U
#define COLOR_TEXT                0xE77CU
#define COLOR_ORANGE              0xF56FU
#define COLOR_ORANGE_DIM          0xB329U
#define COLOR_GREEN               0x6FB7U
#define COLOR_RED                 0xF2CEU
#define COLOR_CYAN                0x67BDU
#define COLOR_DARK                0x18E3U
#define COLOR_DARK_2              0x3166U
#define COLOR_YELLOW              0xFDE0U

static uint16_t fill_line[LCD_W];
static app_asset_status_t asset_status = APP_ASSET_HEADER_ERROR;
static uint8_t last_active_screen = 0xFFU;
static uint8_t last_working = 0xFFU;
static uint8_t anim_frame = 0U;
static uint8_t anim_divider = 0U;
static uint8_t last_valid = 0xFFU;
static uint32_t last_seconds = 0xFFFFFFFFU;
static uint32_t last_sample = 0xFFFFFFFFU;
static uint32_t last_error = 0xFFFFFFFFU;
static uint32_t last_rx = 0xFFFFFFFFU;
static uint32_t last_ai_event = 0xFFFFFFFFU;
static app_ai_mode_t last_ai_mode = (app_ai_mode_t)0xFFU;
static char last_model[16] = "";
static char last_tool[12] = "";
static uint8_t last_context_used = 0xFFU;
static uint8_t last_context_remaining = 0xFFU;
static uint8_t last_limit_used = 0xFFU;
static uint8_t last_limit_remaining = 0xFFU;
static uint16_t last_cost_usd_cents = 0xFFFFU;
static uint8_t last_has_context = 0xFFU;
static uint8_t last_has_limit = 0xFFU;
static uint8_t last_has_cost = 0xFFU;
static dht11_status_t last_dht_status = DHT11_ERROR_CHECKSUM;
static dht11_data_t last_dht_data = {0U, 0U, 0U, 0U};

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void draw_digit(uint16_t x, uint16_t y, uint8_t digit, uint16_t color, uint8_t scale);
static void draw_colon(uint16_t x, uint16_t y, uint16_t color, uint8_t scale);
static void draw_number2(uint16_t x, uint16_t y, uint8_t value, uint16_t color, uint8_t scale);
static void draw_number3(uint16_t x, uint16_t y, uint16_t value, uint16_t color, uint8_t scale);
static void draw_clock_digit(uint16_t x, uint16_t y, uint8_t digit, uint16_t color);
static void draw_clock_colon(uint16_t x, uint16_t y, uint16_t color);
static void draw_clock_number2(uint16_t x, uint16_t y, uint8_t value, uint16_t color);
static void draw_tiny_digit(uint16_t x, uint16_t y, uint8_t digit, uint16_t color);
static void draw_tiny_number3(uint16_t x, uint16_t y, uint16_t value, uint16_t color);
static void draw_tiny_number2(uint16_t x, uint16_t y, uint8_t value, uint16_t color);
static void draw_tiny_money(uint16_t x, uint16_t y, uint16_t cents, uint16_t color);
static void draw_tiny_char(uint16_t x, uint16_t y, char ch, uint16_t color);
static void draw_tiny_text(uint16_t x, uint16_t y, const char *text, uint16_t color, uint8_t max_chars);
static void draw_status_icon(uint8_t working, uint8_t frame);
static void draw_status_animation(uint8_t working, uint8_t frame);
static void draw_idle_icon(void);
static void draw_idle_animation(uint8_t frame);
static void draw_work_icon(void);
static void draw_work_animation(uint8_t frame);
static void draw_clock(uint32_t seconds, uint8_t valid);
static void draw_sensor(const app_sensor_state_t *sensor);
static void draw_counters(const app_sensor_state_t *sensor, const app_clock_state_t *clock);
static void draw_ai_status(const app_ai_state_t *ai);
static const char *mode_text(app_ai_mode_t mode);
static uint16_t mode_color(app_ai_mode_t mode);
static void draw_static_overlay(void);
static void draw_work_overlay(void);
static void invalidate_dynamic_cache(void);

void app_dashboard_init(void)
{
    app_serial_clock_write("[08] dashboard init\r\n");
    app_serial_clock_write("[08] flash asset init...\r\n");
    asset_status = app_flash_assets_init();
    if(APP_ASSET_OK == asset_status) {
        app_serial_clock_write("[08] flash asset ok\r\n");
    } else {
        app_serial_clock_write("[08] flash asset error, fallback screen\r\n");
    }

    app_serial_clock_write("[08] lcd init...\r\n");
    lcd_init();
    if(APP_ASSET_OK == asset_status) {
        app_serial_clock_write("[08] draw external flash background...\r\n");
        asset_status = app_flash_assets_draw_background();
        app_serial_clock_write("[08] background draw done\r\n");
    }
    if(APP_ASSET_OK != asset_status) {
        lcd_clear(COLOR_BG);
    }

    draw_static_overlay();
    app_serial_clock_write("[08] dashboard ready\r\n");
}

void app_dashboard_update(const app_sensor_state_t *sensor,
                          const app_clock_state_t *clock,
                          const app_ai_state_t *ai)
{
    uint8_t ai_working;
    uint8_t active_screen;
    app_ai_mode_t mode;

    if((NULL == sensor) || (NULL == clock)) {
        return;
    }

    mode = (NULL != ai) ? ai->mode : APP_AI_IDLE;
    active_screen = (APP_AI_IDLE != mode) ? 1U : 0U;
    if(last_active_screen != active_screen) {
        if(0U != active_screen) {
            draw_work_overlay();
        } else {
            draw_static_overlay();
        }
        invalidate_dynamic_cache();
        last_active_screen = active_screen;
    }

    ai_working = 0U;
    if(0U != active_screen) {
        ai_working = 1U;
    }

    if(last_working != ai_working) {
        anim_frame = 0U;
        anim_divider = 0U;
        draw_status_icon(ai_working, anim_frame);
    } else {
        anim_divider++;
        if(anim_divider >= 2U) {
            anim_divider = 0U;
            anim_frame = (uint8_t)((anim_frame + 1U) & 0x03U);
            draw_status_animation(ai_working, anim_frame);
        }
    }

    if((last_valid != clock->valid) || (last_seconds != clock->seconds)) {
        draw_clock(clock->seconds, clock->valid);
        last_valid = clock->valid;
        last_seconds = clock->seconds;
    }

    if(0U == active_screen) {
        if((last_dht_status != sensor->status) ||
           (0 != memcmp(&last_dht_data, &sensor->data, sizeof(last_dht_data)))) {
            draw_sensor(sensor);
            last_dht_status = sensor->status;
            last_dht_data = sensor->data;
        }

        if((last_sample != sensor->sample_count) ||
           (last_error != sensor->error_count) ||
           (last_rx != clock->rx_count)) {
            draw_counters(sensor, clock);
            last_sample = sensor->sample_count;
            last_error = sensor->error_count;
            last_rx = clock->rx_count;
        }
    }

    if((NULL != ai) && (0U != active_screen)) {
        if((last_ai_mode != ai->mode) ||
           (last_ai_event != ai->event_count) ||
           (0 != memcmp(last_model, ai->model, sizeof(last_model))) ||
           (0 != memcmp(last_tool, ai->tool, sizeof(last_tool))) ||
           (last_context_used != ai->context_used_percent) ||
           (last_context_remaining != ai->context_remaining_percent) ||
           (last_limit_used != ai->limit_used_percent) ||
           (last_limit_remaining != ai->limit_remaining_percent) ||
           (last_cost_usd_cents != ai->cost_usd_cents) ||
           (last_has_context != ai->has_context) ||
           (last_has_limit != ai->has_limit) ||
           (last_has_cost != ai->has_cost)) {
            draw_ai_status(ai);
            last_ai_mode = ai->mode;
            last_ai_event = ai->event_count;
            (void)memcpy(last_model, ai->model, sizeof(last_model));
            (void)memcpy(last_tool, ai->tool, sizeof(last_tool));
            last_context_used = ai->context_used_percent;
            last_context_remaining = ai->context_remaining_percent;
            last_limit_used = ai->limit_used_percent;
            last_limit_remaining = ai->limit_remaining_percent;
            last_cost_usd_cents = ai->cost_usd_cents;
            last_has_context = ai->has_context;
            last_has_limit = ai->has_limit;
            last_has_cost = ai->has_cost;
        }
    }
}

static void draw_static_overlay(void)
{
    fill_rect(34U, 38U, 172U, 40U, COLOR_PANEL);
    fill_rect(30U, 34U, 180U, 2U, COLOR_ORANGE);
    fill_rect(30U, 39U, 180U, 3U, (APP_ASSET_OK == asset_status) ? COLOR_CYAN : COLOR_RED);

    fill_rect(30U, 196U, 180U, 122U, COLOR_BG);
    fill_rect(36U, 224U, 58U, 27U, COLOR_PANEL_2);
    fill_rect(108U, 224U, 80U, 27U, COLOR_PANEL_2);
    fill_rect(38U, 267U, 160U, 12U, COLOR_PANEL_2);

    draw_clock(0U, 0U);

    if(APP_ASSET_OK == asset_status) {
        fill_rect(82U, 88U, 76U, 4U, COLOR_GREEN);
    } else {
        fill_rect(82U, 88U, 76U, 4U, COLOR_RED);
    }

    draw_status_icon(0U, 0U);
}

static void draw_work_overlay(void)
{
    fill_rect(34U, 38U, 172U, 40U, COLOR_PANEL);
    fill_rect(30U, 34U, 180U, 2U, COLOR_ORANGE);
    fill_rect(30U, 39U, 180U, 3U, COLOR_CYAN);

    fill_rect(30U, 196U, 180U, 122U, COLOR_BG);
    fill_rect(38U, 198U, 164U, 28U, COLOR_PANEL);
    fill_rect(38U, 232U, 164U, 22U, COLOR_PANEL_2);
    fill_rect(38U, 260U, 164U, 22U, COLOR_PANEL_2);
    fill_rect(38U, 288U, 164U, 22U, COLOR_PANEL_2);

    draw_clock(0U, 0U);
    draw_status_icon(1U, 0U);
}

static void draw_status_icon(uint8_t working, uint8_t frame)
{
    fill_rect(30U, 82U, 180U, 112U, COLOR_BG);
    fill_rect(38U, 90U, 164U, 96U, 0x0021U);

    if(0U != working) {
        draw_work_icon();
    } else {
        draw_idle_icon();
    }

    draw_status_animation(working, frame);
    last_working = (0U != working) ? 1U : 0U;
}

static void draw_status_animation(uint8_t working, uint8_t frame)
{
    if(0U != working) {
        draw_work_animation(frame);
    } else {
        draw_idle_animation(frame);
    }
}

static void draw_idle_icon(void)
{
    uint16_t body = COLOR_ORANGE;

    /* idle mascot with headphones */
    fill_rect(72U, 116U, 96U, 48U, body);
    fill_rect(58U, 130U, 16U, 34U, body);
    fill_rect(166U, 130U, 16U, 34U, body);
    fill_rect(70U, 164U, 12U, 24U, body);
    fill_rect(96U, 164U, 12U, 24U, body);
    fill_rect(132U, 164U, 12U, 24U, body);
    fill_rect(158U, 164U, 12U, 24U, body);

    fill_rect(72U, 154U, 96U, 10U, body);
    fill_rect(58U, 158U, 16U, 6U, body);
    fill_rect(166U, 158U, 16U, 6U, body);

    fill_rect(88U, 133U, 10U, 11U, COLOR_BG);
    fill_rect(142U, 133U, 10U, 11U, COLOR_BG);

    fill_rect(76U, 101U, 88U, 6U, COLOR_DARK);
    fill_rect(68U, 107U, 8U, 8U, COLOR_DARK);
    fill_rect(164U, 107U, 8U, 8U, COLOR_DARK);
    fill_rect(60U, 115U, 12U, 18U, COLOR_DARK);
    fill_rect(168U, 115U, 12U, 18U, COLOR_DARK);
    fill_rect(52U, 129U, 18U, 38U, COLOR_DARK);
    fill_rect(170U, 129U, 18U, 38U, COLOR_DARK);
    fill_rect(56U, 135U, 7U, 24U, COLOR_DARK_2);
    fill_rect(177U, 135U, 7U, 24U, COLOR_DARK_2);
}

static void draw_idle_animation(uint8_t frame)
{
    uint16_t note_color = (0U != (frame & 0x01U)) ? COLOR_CYAN : COLOR_TEXT;
    uint16_t note_color_2 = (0U != (frame & 0x02U)) ? COLOR_ORANGE : COLOR_TEXT;

    fill_rect(176U, 92U, 36U, 68U, 0x0021U);
    fill_rect(76U, 101U, 88U, 6U, (0U != (frame & 0x01U)) ? COLOR_DARK_2 : COLOR_DARK);

    if(0U != (frame & 0x01U)) {
        fill_rect(184U, 96U, 5U, 24U, note_color);
        fill_rect(189U, 96U, 13U, 5U, note_color);
        fill_rect(178U, 116U, 11U, 8U, note_color);
    } else {
        fill_rect(184U, 104U, 5U, 24U, note_color);
        fill_rect(189U, 104U, 13U, 5U, note_color);
        fill_rect(178U, 124U, 11U, 8U, note_color);
    }

    if(0U != (frame & 0x02U)) {
        fill_rect(198U, 127U, 4U, 18U, note_color_2);
        fill_rect(202U, 127U, 9U, 4U, note_color_2);
        fill_rect(192U, 142U, 10U, 8U, note_color_2);
    } else {
        fill_rect(196U, 136U, 4U, 18U, note_color_2);
        fill_rect(200U, 136U, 9U, 4U, note_color_2);
        fill_rect(190U, 151U, 10U, 8U, note_color_2);
    }
}

static void draw_work_icon(void)
{
    uint16_t body = COLOR_ORANGE;

    /* working mascot with laptop */
    fill_rect(58U, 112U, 104U, 50U, body);
    fill_rect(46U, 126U, 14U, 34U, body);
    fill_rect(160U, 126U, 14U, 34U, body);
    fill_rect(54U, 162U, 12U, 23U, body);
    fill_rect(80U, 162U, 12U, 23U, body);
    fill_rect(116U, 162U, 12U, 23U, body);
    fill_rect(142U, 162U, 12U, 23U, body);

    fill_rect(58U, 152U, 104U, 10U, body);
    fill_rect(46U, 154U, 14U, 6U, body);
    fill_rect(160U, 154U, 14U, 6U, body);

    fill_rect(86U, 128U, 10U, 11U, COLOR_BG);
    fill_rect(139U, 128U, 10U, 11U, COLOR_BG);

    fill_rect(112U, 136U, 74U, 38U, COLOR_DARK);
    fill_rect(118U, 142U, 62U, 26U, COLOR_DARK_2);
    fill_rect(110U, 174U, 80U, 8U, COLOR_DARK);
    fill_rect(96U, 166U, 20U, 12U, COLOR_DARK);
}

static void draw_work_animation(uint8_t frame)
{
    uint16_t code_color = (0U != (frame & 0x01U)) ? COLOR_CYAN : COLOR_GREEN;
    uint16_t code_color_2 = (0U != (frame & 0x02U)) ? COLOR_GREEN : COLOR_CYAN;

    fill_rect(124U, 144U, 54U, 25U, COLOR_DARK_2);
    fill_rect(174U, 86U, 38U, 4U, COLOR_BG);
    fill_rect(174U, 90U, 38U, 30U, 0x0021U);

    fill_rect(126U, 152U, 7U, 5U, code_color);
    fill_rect(133U, 147U, 7U, 15U, code_color);
    fill_rect(148U, 147U, 7U, 20U, code_color_2);
    fill_rect(142U, 162U, 13U, 5U, code_color_2);
    if(0U != (frame & 0x01U)) {
        fill_rect(162U, 146U, 7U, 5U, code_color);
        fill_rect(169U, 151U, 7U, 5U, code_color);
        fill_rect(162U, 156U, 7U, 5U, code_color);
    } else {
        fill_rect(162U, 150U, 7U, 5U, code_color);
        fill_rect(169U, 155U, 7U, 5U, code_color);
        fill_rect(162U, 160U, 7U, 5U, code_color);
    }

    if(0U != (frame & 0x02U)) {
        fill_rect(185U, 88U, 8U, 8U, COLOR_YELLOW);
        fill_rect(177U, 96U, 8U, 8U, COLOR_YELLOW);
        fill_rect(185U, 96U, 8U, 8U, COLOR_BG);
        fill_rect(193U, 96U, 8U, 8U, COLOR_YELLOW);
        fill_rect(185U, 104U, 8U, 8U, COLOR_YELLOW);
    } else {
        fill_rect(183U, 92U, 8U, 8U, COLOR_YELLOW);
        fill_rect(175U, 100U, 8U, 8U, COLOR_YELLOW);
        fill_rect(183U, 100U, 8U, 8U, COLOR_BG);
        fill_rect(191U, 100U, 8U, 8U, COLOR_YELLOW);
        fill_rect(183U, 108U, 8U, 8U, COLOR_YELLOW);
    }
}

static void draw_clock(uint32_t seconds, uint8_t valid)
{
    uint8_t h;
    uint8_t m;
    uint8_t s;
    uint16_t color;

    fill_rect(36U, 43U, 168U, 34U, COLOR_PANEL);
    color = (0U != valid) ? COLOR_ORANGE : COLOR_ORANGE_DIM;

    if(0U == valid) {
        fill_rect(62U, 58U, 18U, 3U, color);
        fill_rect(92U, 58U, 18U, 3U, color);
        fill_rect(122U, 58U, 18U, 3U, color);
        fill_rect(152U, 58U, 18U, 3U, color);
        return;
    }

    h = (uint8_t)(seconds / 3600U);
    m = (uint8_t)((seconds / 60U) % 60U);
    s = (uint8_t)(seconds % 60U);

    draw_clock_number2(43U, 45U, h, color);
    draw_clock_colon(86U, 47U, color);
    draw_clock_number2(99U, 45U, m, color);
    draw_clock_colon(142U, 47U, color);
    draw_clock_number2(155U, 45U, s, color);
}

static void draw_sensor(const app_sensor_state_t *sensor)
{
    uint8_t temp;
    uint8_t humi;

    fill_rect(36U, 224U, 58U, 27U, COLOR_PANEL_2);
    fill_rect(108U, 224U, 80U, 27U, COLOR_PANEL_2);

    if(DHT11_OK == sensor->status) {
        temp = (sensor->data.temperature_int > 99U) ? 99U : sensor->data.temperature_int;
        humi = (sensor->data.humidity_int > 99U) ? 99U : sensor->data.humidity_int;
        draw_number2(49U, 225U, temp, COLOR_TEXT, 1U);
        draw_number2(132U, 225U, humi, COLOR_CYAN, 1U);
        fill_rect(194U, 229U, 12U, 12U, COLOR_GREEN);
    } else {
        fill_rect(48U, 236U, 32U, 4U, COLOR_RED);
        fill_rect(124U, 236U, 44U, 4U, COLOR_RED);
        fill_rect(194U, 229U, 12U, 12U, COLOR_RED);
    }
}

static void draw_counters(const app_sensor_state_t *sensor, const app_clock_state_t *clock)
{
    fill_rect(38U, 267U, 160U, 12U, COLOR_PANEL_2);
    draw_tiny_number3(42U, 268U, (uint16_t)(sensor->sample_count % 1000U), COLOR_GREEN);
    draw_tiny_number3(98U, 268U, (uint16_t)(sensor->error_count % 1000U), COLOR_RED);
    draw_tiny_number3(154U, 268U, (uint16_t)(clock->rx_count % 1000U), COLOR_ORANGE);
}

static void draw_ai_status(const app_ai_state_t *ai)
{
    uint16_t color;

    if(NULL == ai) {
        return;
    }

    color = mode_color(ai->mode);

    fill_rect(38U, 198U, 164U, 28U, COLOR_PANEL);
    fill_rect(42U, 201U, 156U, 5U, color);
    if(APP_AI_WAIT_APPROVE == ai->mode) {
        draw_tiny_text(48U, 212U, "PA0 OK PA4 NO", COLOR_YELLOW, 14U);
    } else if(APP_AI_NATIVE_APPROVE == ai->mode) {
        draw_tiny_text(48U, 212U, "TERM 123", COLOR_CYAN, 8U);
    } else {
        draw_tiny_text(48U, 212U, mode_text(ai->mode), color, 7U);
    }
    if(APP_AI_WORKING == ai->mode) {
        draw_tiny_text(104U, 212U, ai->tool, COLOR_CYAN, 10U);
    }

    fill_rect(38U, 232U, 164U, 22U, COLOR_PANEL_2);
    draw_tiny_text(42U, 238U, "M:", COLOR_TEXT, 2U);
    draw_tiny_text(58U, 238U, ai->model, COLOR_CYAN, 16U);

    fill_rect(38U, 260U, 164U, 22U, COLOR_PANEL_2);
    draw_tiny_text(42U, 266U, "CTX", COLOR_TEXT, 3U);
    if(0U != ai->has_context) {
        draw_tiny_number3(74U, 266U, ai->context_used_percent, COLOR_ORANGE);
        draw_tiny_char(100U, 266U, '%', COLOR_ORANGE);
        draw_tiny_text(112U, 266U, "R", COLOR_TEXT, 1U);
        draw_tiny_number3(124U, 266U, ai->context_remaining_percent, COLOR_GREEN);
        draw_tiny_char(150U, 266U, '%', COLOR_GREEN);
    } else {
        draw_tiny_text(74U, 266U, "---", COLOR_ORANGE_DIM, 3U);
    }

    fill_rect(38U, 288U, 164U, 22U, COLOR_PANEL_2);
    if(0U != ai->has_cost) {
        draw_tiny_text(42U, 294U, "USD", COLOR_TEXT, 3U);
        draw_tiny_money(74U, 294U, ai->cost_usd_cents, COLOR_GREEN);
    } else if(0U != ai->has_limit) {
        draw_tiny_text(42U, 294U, "LIM", COLOR_TEXT, 3U);
        draw_tiny_number3(74U, 294U, ai->limit_used_percent, COLOR_RED);
        draw_tiny_char(100U, 294U, '%', COLOR_RED);
        draw_tiny_text(112U, 294U, "R", COLOR_TEXT, 1U);
        draw_tiny_number3(124U, 294U, ai->limit_remaining_percent, COLOR_GREEN);
        draw_tiny_char(150U, 294U, '%', COLOR_GREEN);
    } else {
        draw_tiny_text(42U, 294U, "USD", COLOR_TEXT, 3U);
        draw_tiny_text(74U, 294U, "---", COLOR_ORANGE_DIM, 3U);
    }
}

static const char *mode_text(app_ai_mode_t mode)
{
    switch(mode) {
    case APP_AI_WORKING:
        return "WORK";
    case APP_AI_NATIVE_APPROVE:
        return "TERM";
    case APP_AI_WAIT_APPROVE:
        return "WAIT";
    case APP_AI_APPROVED:
        return "ALLOW";
    case APP_AI_DENIED:
        return "DENY";
    case APP_AI_ERROR:
        return "ERROR";
    case APP_AI_IDLE:
    default:
        return "IDLE";
    }
}

static uint16_t mode_color(app_ai_mode_t mode)
{
    switch(mode) {
    case APP_AI_WORKING:
        return COLOR_CYAN;
    case APP_AI_NATIVE_APPROVE:
        return COLOR_CYAN;
    case APP_AI_WAIT_APPROVE:
        return COLOR_YELLOW;
    case APP_AI_APPROVED:
        return COLOR_GREEN;
    case APP_AI_DENIED:
    case APP_AI_ERROR:
        return COLOR_RED;
    case APP_AI_IDLE:
    default:
        return COLOR_ORANGE;
    }
}

static void invalidate_dynamic_cache(void)
{
    last_working = 0xFFU;
    last_valid = 0xFFU;
    last_seconds = 0xFFFFFFFFU;
    last_sample = 0xFFFFFFFFU;
    last_error = 0xFFFFFFFFU;
    last_rx = 0xFFFFFFFFU;
    last_ai_event = 0xFFFFFFFFU;
    last_ai_mode = (app_ai_mode_t)0xFFU;
    last_model[0] = '\0';
    last_tool[0] = '\0';
    last_context_used = 0xFFU;
    last_context_remaining = 0xFFU;
    last_limit_used = 0xFFU;
    last_limit_remaining = 0xFFU;
    last_cost_usd_cents = 0xFFFFU;
    last_has_context = 0xFFU;
    last_has_limit = 0xFFU;
    last_has_cost = 0xFFU;
    last_dht_status = DHT11_ERROR_CHECKSUM;
    (void)memset(&last_dht_data, 0xFF, sizeof(last_dht_data));
}

static void draw_number2(uint16_t x, uint16_t y, uint8_t value, uint16_t color, uint8_t scale)
{
    draw_digit(x, y, (uint8_t)(value / 10U), color, scale);
    draw_digit((uint16_t)(x + (16U * scale)), y, (uint8_t)(value % 10U), color, scale);
}

static void draw_number3(uint16_t x, uint16_t y, uint16_t value, uint16_t color, uint8_t scale)
{
    draw_digit(x, y, (uint8_t)((value / 100U) % 10U), color, scale);
    draw_digit((uint16_t)(x + (12U * scale)), y, (uint8_t)((value / 10U) % 10U), color, scale);
    draw_digit((uint16_t)(x + (24U * scale)), y, (uint8_t)(value % 10U), color, scale);
}

static void draw_colon(uint16_t x, uint16_t y, uint16_t color, uint8_t scale)
{
    fill_rect(x, (uint16_t)(y + (5U * scale)), (uint16_t)(2U * scale), (uint16_t)(2U * scale), color);
    fill_rect(x, (uint16_t)(y + (14U * scale)), (uint16_t)(2U * scale), (uint16_t)(2U * scale), color);
}

static void draw_digit(uint16_t x, uint16_t y, uint8_t digit, uint16_t color, uint8_t scale)
{
    static const uint8_t seg_map[10] = {
        0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U,
        0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU
    };
    uint8_t seg;
    uint16_t t = (uint16_t)(2U * scale);
    uint16_t w = (uint16_t)(8U * scale);
    uint16_t h = (uint16_t)(10U * scale);

    if(digit > 9U) {
        return;
    }

    seg = seg_map[digit];
    if(0U != (seg & 0x01U)) fill_rect((uint16_t)(x + t), y, w, t, color);
    if(0U != (seg & 0x02U)) fill_rect((uint16_t)(x + t + w), (uint16_t)(y + t), t, h, color);
    if(0U != (seg & 0x04U)) fill_rect((uint16_t)(x + t + w), (uint16_t)(y + (2U * t) + h), t, h, color);
    if(0U != (seg & 0x08U)) fill_rect((uint16_t)(x + t), (uint16_t)(y + (2U * h) + (2U * t)), w, t, color);
    if(0U != (seg & 0x10U)) fill_rect(x, (uint16_t)(y + (2U * t) + h), t, h, color);
    if(0U != (seg & 0x20U)) fill_rect(x, (uint16_t)(y + t), t, h, color);
    if(0U != (seg & 0x40U)) fill_rect((uint16_t)(x + t), (uint16_t)(y + h + t), w, t, color);
}

static void draw_clock_number2(uint16_t x, uint16_t y, uint8_t value, uint16_t color)
{
    draw_clock_digit(x, y, (uint8_t)(value / 10U), color);
    draw_clock_digit((uint16_t)(x + 21U), y, (uint8_t)(value % 10U), color);
}

static void draw_clock_colon(uint16_t x, uint16_t y, uint16_t color)
{
    fill_rect(x, (uint16_t)(y + 8U), 3U, 3U, color);
    fill_rect(x, (uint16_t)(y + 20U), 3U, 3U, color);
}

static void draw_clock_digit(uint16_t x, uint16_t y, uint8_t digit, uint16_t color)
{
    static const uint8_t seg_map[10] = {
        0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U,
        0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU
    };
    uint8_t seg;

    if(digit > 9U) {
        return;
    }

    seg = seg_map[digit];
    if(0U != (seg & 0x01U)) fill_rect((uint16_t)(x + 3U), y, 10U, 3U, color);
    if(0U != (seg & 0x02U)) fill_rect((uint16_t)(x + 13U), (uint16_t)(y + 3U), 3U, 11U, color);
    if(0U != (seg & 0x04U)) fill_rect((uint16_t)(x + 13U), (uint16_t)(y + 17U), 3U, 11U, color);
    if(0U != (seg & 0x08U)) fill_rect((uint16_t)(x + 3U), (uint16_t)(y + 28U), 10U, 3U, color);
    if(0U != (seg & 0x10U)) fill_rect(x, (uint16_t)(y + 17U), 3U, 11U, color);
    if(0U != (seg & 0x20U)) fill_rect(x, (uint16_t)(y + 3U), 3U, 11U, color);
    if(0U != (seg & 0x40U)) fill_rect((uint16_t)(x + 3U), (uint16_t)(y + 14U), 10U, 3U, color);
}

static void draw_tiny_number3(uint16_t x, uint16_t y, uint16_t value, uint16_t color)
{
    draw_tiny_digit(x, y, (uint8_t)((value / 100U) % 10U), color);
    draw_tiny_digit((uint16_t)(x + 8U), y, (uint8_t)((value / 10U) % 10U), color);
    draw_tiny_digit((uint16_t)(x + 16U), y, (uint8_t)(value % 10U), color);
}

static void draw_tiny_number2(uint16_t x, uint16_t y, uint8_t value, uint16_t color)
{
    draw_tiny_digit(x, y, (uint8_t)((value / 10U) % 10U), color);
    draw_tiny_digit((uint16_t)(x + 8U), y, (uint8_t)(value % 10U), color);
}

static void draw_tiny_money(uint16_t x, uint16_t y, uint16_t cents, uint16_t color)
{
    uint16_t dollars = (uint16_t)(cents / 100U);
    uint8_t cent_part = (uint8_t)(cents % 100U);

    if(dollars > 999U) {
        dollars = 999U;
    }

    draw_tiny_number3(x, y, dollars, color);
    draw_tiny_char((uint16_t)(x + 24U), y, '.', color);
    draw_tiny_number2((uint16_t)(x + 32U), y, cent_part, color);
}

static void draw_tiny_digit(uint16_t x, uint16_t y, uint8_t digit, uint16_t color)
{
    static const uint8_t glyphs[10][5] = {
        {0x07U, 0x05U, 0x05U, 0x05U, 0x07U},
        {0x02U, 0x06U, 0x02U, 0x02U, 0x07U},
        {0x07U, 0x01U, 0x07U, 0x04U, 0x07U},
        {0x07U, 0x01U, 0x07U, 0x01U, 0x07U},
        {0x05U, 0x05U, 0x07U, 0x01U, 0x01U},
        {0x07U, 0x04U, 0x07U, 0x01U, 0x07U},
        {0x07U, 0x04U, 0x07U, 0x05U, 0x07U},
        {0x07U, 0x01U, 0x02U, 0x02U, 0x02U},
        {0x07U, 0x05U, 0x07U, 0x05U, 0x07U},
        {0x07U, 0x05U, 0x07U, 0x01U, 0x07U}
    };
    uint8_t row;
    uint8_t col;

    if(digit > 9U) {
        return;
    }

    for(row = 0U; row < 5U; row++) {
        for(col = 0U; col < 3U; col++) {
            if(0U != (glyphs[digit][row] & (uint8_t)(1U << (2U - col)))) {
                fill_rect((uint16_t)(x + (col * 2U)), (uint16_t)(y + (row * 2U)), 2U, 2U, color);
            }
        }
    }
}

static void draw_tiny_text(uint16_t x, uint16_t y, const char *text, uint16_t color, uint8_t max_chars)
{
    uint8_t count = 0U;

    if(NULL == text) {
        return;
    }

    while(('\0' != *text) && (count < max_chars)) {
        draw_tiny_char((uint16_t)(x + (count * 8U)), y, *text, color);
        text++;
        count++;
    }
}

static void draw_tiny_char(uint16_t x, uint16_t y, char ch, uint16_t color)
{
    uint8_t rows[5] = {0U, 0U, 0U, 0U, 0U};
    uint8_t row;
    uint8_t col;

    if((ch >= '0') && (ch <= '9')) {
        draw_tiny_digit(x, y, (uint8_t)(ch - '0'), color);
        return;
    }

    switch(ch) {
    case 'A': rows[0]=0x02U; rows[1]=0x05U; rows[2]=0x07U; rows[3]=0x05U; rows[4]=0x05U; break;
    case 'B': rows[0]=0x06U; rows[1]=0x05U; rows[2]=0x06U; rows[3]=0x05U; rows[4]=0x06U; break;
    case 'C': rows[0]=0x07U; rows[1]=0x04U; rows[2]=0x04U; rows[3]=0x04U; rows[4]=0x07U; break;
    case 'D': rows[0]=0x06U; rows[1]=0x05U; rows[2]=0x05U; rows[3]=0x05U; rows[4]=0x06U; break;
    case 'E': rows[0]=0x07U; rows[1]=0x04U; rows[2]=0x06U; rows[3]=0x04U; rows[4]=0x07U; break;
    case 'F': rows[0]=0x07U; rows[1]=0x04U; rows[2]=0x06U; rows[3]=0x04U; rows[4]=0x04U; break;
    case 'G': rows[0]=0x07U; rows[1]=0x04U; rows[2]=0x05U; rows[3]=0x05U; rows[4]=0x07U; break;
    case 'H': rows[0]=0x05U; rows[1]=0x05U; rows[2]=0x07U; rows[3]=0x05U; rows[4]=0x05U; break;
    case 'I': rows[0]=0x07U; rows[1]=0x02U; rows[2]=0x02U; rows[3]=0x02U; rows[4]=0x07U; break;
    case 'J': rows[0]=0x01U; rows[1]=0x01U; rows[2]=0x01U; rows[3]=0x05U; rows[4]=0x07U; break;
    case 'K': rows[0]=0x05U; rows[1]=0x05U; rows[2]=0x06U; rows[3]=0x05U; rows[4]=0x05U; break;
    case 'L': rows[0]=0x04U; rows[1]=0x04U; rows[2]=0x04U; rows[3]=0x04U; rows[4]=0x07U; break;
    case 'M': rows[0]=0x05U; rows[1]=0x07U; rows[2]=0x07U; rows[3]=0x05U; rows[4]=0x05U; break;
    case 'N': rows[0]=0x05U; rows[1]=0x07U; rows[2]=0x07U; rows[3]=0x07U; rows[4]=0x05U; break;
    case 'O': rows[0]=0x07U; rows[1]=0x05U; rows[2]=0x05U; rows[3]=0x05U; rows[4]=0x07U; break;
    case 'P': rows[0]=0x07U; rows[1]=0x05U; rows[2]=0x07U; rows[3]=0x04U; rows[4]=0x04U; break;
    case 'Q': rows[0]=0x07U; rows[1]=0x05U; rows[2]=0x05U; rows[3]=0x07U; rows[4]=0x01U; break;
    case 'R': rows[0]=0x07U; rows[1]=0x05U; rows[2]=0x07U; rows[3]=0x06U; rows[4]=0x05U; break;
    case 'S': rows[0]=0x07U; rows[1]=0x04U; rows[2]=0x07U; rows[3]=0x01U; rows[4]=0x07U; break;
    case 'T': rows[0]=0x07U; rows[1]=0x02U; rows[2]=0x02U; rows[3]=0x02U; rows[4]=0x02U; break;
    case 'U': rows[0]=0x05U; rows[1]=0x05U; rows[2]=0x05U; rows[3]=0x05U; rows[4]=0x07U; break;
    case 'V': rows[0]=0x05U; rows[1]=0x05U; rows[2]=0x05U; rows[3]=0x05U; rows[4]=0x02U; break;
    case 'W': rows[0]=0x05U; rows[1]=0x05U; rows[2]=0x07U; rows[3]=0x07U; rows[4]=0x05U; break;
    case 'X': rows[0]=0x05U; rows[1]=0x05U; rows[2]=0x02U; rows[3]=0x05U; rows[4]=0x05U; break;
    case 'Y': rows[0]=0x05U; rows[1]=0x05U; rows[2]=0x02U; rows[3]=0x02U; rows[4]=0x02U; break;
    case 'Z': rows[0]=0x07U; rows[1]=0x01U; rows[2]=0x02U; rows[3]=0x04U; rows[4]=0x07U; break;
    case '-': rows[2]=0x07U; break;
    case '_': rows[4]=0x07U; break;
    case '.': rows[4]=0x02U; break;
    case ':': rows[1]=0x02U; rows[3]=0x02U; break;
    case '%': rows[0]=0x05U; rows[1]=0x01U; rows[2]=0x02U; rows[3]=0x04U; rows[4]=0x05U; break;
    case ' ':
    default:
        return;
    }

    for(row = 0U; row < 5U; row++) {
        for(col = 0U; col < 3U; col++) {
            if(0U != (rows[row] & (uint8_t)(1U << (2U - col)))) {
                fill_rect((uint16_t)(x + (col * 2U)), (uint16_t)(y + (row * 2U)), 2U, 2U, color);
            }
        }
    }
}

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint16_t row;
    uint16_t col;

    if((0U == w) || (0U == h) || (x >= LCD_W) || (y >= LCD_H)) {
        return;
    }
    if((x + w) > LCD_W) {
        w = (uint16_t)(LCD_W - x);
    }
    if((y + h) > LCD_H) {
        h = (uint16_t)(LCD_H - y);
    }

    for(col = 0U; col < w; col++) {
        fill_line[col] = color;
    }

    for(row = 0U; row < h; row++) {
        lcd_flush_area(x, (uint16_t)(y + row), (uint16_t)(x + w - 1U), (uint16_t)(y + row), fill_line);
    }
}
