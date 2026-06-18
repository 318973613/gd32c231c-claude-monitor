#include "app_dashboard.h"
#include "lvgl.h"
#include "lcd_driver.h"

#define LCD_HOR_RES              240U
#define LCD_VER_RES              320U
#define LVGL_BUFFER_LINES        2U

static lv_disp_draw_buf_t draw_buf;
static lv_color_t lvgl_buf[LCD_HOR_RES * LVGL_BUFFER_LINES];

static lv_obj_t *clock_label;
static lv_obj_t *env_label;
static lv_obj_t *state_label;
static lv_obj_t *serial_label;
static lv_obj_t *footer_label;

static void lvgl_port_init(void);
static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void create_screen(void);

void app_dashboard_init(void)
{
    lcd_init();
    lcd_clear(BLACK);
    lv_init();
    lvgl_port_init();
    create_screen();
}

void app_dashboard_update(const app_sensor_state_t *sensor, const app_clock_state_t *clock)
{
    uint16_t temperature_x10;
    uint16_t humidity_x10;
    uint32_t h;
    uint32_t m;
    uint32_t s;

    if((NULL == sensor) || (NULL == clock)) {
        return;
    }

    if(0U != clock->valid) {
        h = clock->seconds / 3600U;
        m = (clock->seconds / 60U) % 60U;
        s = clock->seconds % 60U;
        lv_label_set_text_fmt(clock_label, "%02lu:%02lu:%02lu",
                              (unsigned long)h,
                              (unsigned long)m,
                              (unsigned long)s);
        lv_obj_set_style_text_color(clock_label, lv_color_hex(0xFFB084), 0);
    } else {
        lv_label_set_text(clock_label, "--:--:--");
        lv_obj_set_style_text_color(clock_label, lv_color_hex(0xC87B5B), 0);
    }

    if(DHT11_OK == sensor->status) {
        temperature_x10 = ((uint16_t)sensor->data.temperature_int * 10U) + sensor->data.temperature_dec;
        humidity_x10 = ((uint16_t)sensor->data.humidity_int * 10U) + sensor->data.humidity_dec;

        lv_label_set_text_fmt(env_label,
                              "TEMP %u.%u C  HUMI %u.%u %%",
                              temperature_x10 / 10U,
                              temperature_x10 % 10U,
                              humidity_x10 / 10U,
                              humidity_x10 % 10U);
        lv_label_set_text(state_label, "DHT11 PB0  OK");
        lv_obj_set_style_text_color(state_label, lv_color_hex(0x7BE0A3), 0);
    } else {
        lv_label_set_text(env_label, "TEMP --.- C  HUMI --.- %");
        lv_label_set_text_fmt(state_label, "DHT11 PB0  %s", dht11_status_string(sensor->status));
        lv_obj_set_style_text_color(state_label, lv_color_hex(0xFF6B6B), 0);
    }

    lv_label_set_text_fmt(serial_label,
                          "USART0 TIME=%s SET:%lu",
                          (0U != clock->valid) ? "OK" : "?",
                          (unsigned long)clock->set_count);
    lv_label_set_text_fmt(footer_label,
                          "S:%lu E:%lu RX:%lu",
                          (unsigned long)sensor->sample_count,
                          (unsigned long)sensor->error_count,
                          (unsigned long)clock->rx_count);
}

static void lvgl_port_init(void)
{
    static lv_disp_drv_t disp_drv;

    lv_disp_draw_buf_init(&draw_buf, lvgl_buf, NULL, LCD_HOR_RES * LVGL_BUFFER_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_HOR_RES;
    disp_drv.ver_res = LCD_VER_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    (void)lv_disp_drv_register(&disp_drv);
}

static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    if((area->x2 < 0) || (area->y2 < 0) ||
       (area->x1 >= (lv_coord_t)LCD_HOR_RES) || (area->y1 >= (lv_coord_t)LCD_VER_RES)) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    lcd_flush_area((uint16_t)area->x1,
                   (uint16_t)area->y1,
                   (uint16_t)area->x2,
                   (uint16_t)area->y2,
                   (const uint16_t *)color_p);

    lv_disp_flush_ready(disp_drv);
}

static void create_screen(void)
{
    lv_obj_t *screen;
    lv_obj_t *title_label;
    lv_obj_t *mascot_label;

    screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080706), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    title_label = lv_label_create(screen);
    lv_label_set_text(title_label, "Claude Desk");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFF2E8), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 24);

    clock_label = lv_label_create(screen);
    lv_label_set_text(clock_label, "--:--:--");
    lv_obj_set_style_text_color(clock_label, lv_color_hex(0xFF9C6E), 0);
    lv_obj_align(clock_label, LV_ALIGN_TOP_MID, 0, 52);

    mascot_label = lv_label_create(screen);
    lv_label_set_text(mascot_label,
                      "  ######  \n"
                      " ######## \n"
                      "## #  # ##\n"
                      "##########\n"
                      " ######## \n"
                      "  ##  ##  ");
    lv_obj_set_style_text_color(mascot_label, lv_color_hex(0xD97757), 0);
    lv_obj_align(mascot_label, LV_ALIGN_CENTER, 0, -42);

    env_label = lv_label_create(screen);
    lv_label_set_text(env_label, "TEMP --.- C  HUMI --.- %");
    lv_obj_set_style_text_color(env_label, lv_color_hex(0xFFE0C8), 0);
    lv_obj_align(env_label, LV_ALIGN_CENTER, 0, 42);

    state_label = lv_label_create(screen);
    lv_label_set_text(state_label, "DHT11 PB0  WAIT");
    lv_obj_set_style_text_color(state_label, lv_color_hex(0xD6A85D), 0);
    lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 70);

    serial_label = lv_label_create(screen);
    lv_label_set_text(serial_label, "USART0  TIME?");
    lv_obj_set_style_text_color(serial_label, lv_color_hex(0x8F7B6A), 0);
    lv_obj_align(serial_label, LV_ALIGN_BOTTOM_MID, 0, -36);

    footer_label = lv_label_create(screen);
    lv_label_set_text(footer_label, "S:0 E:0 RX:0");
    lv_obj_set_style_text_color(footer_label, lv_color_hex(0xFFF2E8), 0);
    lv_obj_align(footer_label, LV_ALIGN_BOTTOM_MID, 0, -14);
}
