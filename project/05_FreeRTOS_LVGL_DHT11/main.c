/*!
    \file    main.c
    \brief   FreeRTOS + LVGL display demo for GD32C231C-EVAL

    \version 2026-06-17, custom LVGL demo for gd32c2x1
*/

#include "gd32c2x1.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "lcd_driver.h"
#include "dht11.h"

#define LCD_HOR_RES              240U
#define LCD_VER_RES              320U
#define LVGL_BUFFER_LINES        2U
#define GUI_TASK_STACK_WORDS     768U
#define GUI_TASK_PRIORITY        (tskIDLE_PRIORITY + 2U)
#define DHT_TASK_STACK_WORDS     160U
#define DHT_TASK_PRIORITY        (tskIDLE_PRIORITY + 1U)
#define DHT_SAMPLE_PERIOD_MS     2000U
#define GUI_UPDATE_PERIOD_MS     500U

typedef struct {
    dht11_data_t data;
    dht11_status_t status;
    uint32_t sample_count;
    uint32_t error_count;
} sensor_state_t;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t lvgl_buf[LCD_HOR_RES * LVGL_BUFFER_LINES];
static sensor_state_t sensor_state = {{0U, 0U, 0U, 0U}, DHT11_ERROR_TIMEOUT, 0U, 0U};
static lv_obj_t *temp_label;
static lv_obj_t *humi_label;
static lv_obj_t *state_label;
static lv_obj_t *count_label;

static void gui_task(void *argument);
static void dht11_task(void *argument);
static void lvgl_port_init(void);
static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void create_demo_screen(void);
static void update_demo_screen(void);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    if(pdPASS != xTaskCreate(gui_task, "gui", GUI_TASK_STACK_WORDS, NULL, GUI_TASK_PRIORITY, NULL)) {
        while(1) {
        }
    }

    if(pdPASS != xTaskCreate(dht11_task, "dht11", DHT_TASK_STACK_WORDS, NULL, DHT_TASK_PRIORITY, NULL)) {
        while(1) {
        }
    }

    vTaskStartScheduler();

    while(1) {
    }
}

/*!
    \brief      GUI task: initialize LCD/LVGL and run LVGL timer handler
    \param[in]  argument: unused
    \param[out] none
    \retval     none
*/
static void gui_task(void *argument)
{
    TickType_t last_update;

    (void)argument;

    lcd_init();
    lcd_clear(BLACK);
    lv_init();
    lvgl_port_init();
    create_demo_screen();
    last_update = xTaskGetTickCount();

    while(1) {
        lv_timer_handler();

        if((xTaskGetTickCount() - last_update) >= pdMS_TO_TICKS(GUI_UPDATE_PERIOD_MS)) {
            last_update = xTaskGetTickCount();
            update_demo_screen();
        }

        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}

static void dht11_task(void *argument)
{
    dht11_data_t data;
    dht11_status_t status;

    (void)argument;

    dht11_init();
    vTaskDelay(pdMS_TO_TICKS(1500U));

    while(1) {
        status = dht11_read(&data);

        taskENTER_CRITICAL();
        sensor_state.status = status;
        sensor_state.sample_count++;
        if(DHT11_OK == status) {
            sensor_state.data = data;
        } else {
            sensor_state.error_count++;
        }
        taskEXIT_CRITICAL();

        vTaskDelay(pdMS_TO_TICKS(DHT_SAMPLE_PERIOD_MS));
    }
}

/*!
    \brief      initialize LVGL display driver
    \param[in]  none
    \param[out] none
    \retval     none
*/
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

/*!
    \brief      LVGL display flush callback
    \param[in]  disp_drv: LVGL display driver
    \param[in]  area: area to refresh
    \param[in]  color_p: RGB565 pixel data
    \param[out] none
    \retval     none
*/
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

/*!
    \brief      create a simple LVGL screen
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void create_demo_screen(void)
{
    lv_obj_t *screen;
    lv_obj_t *title;
    lv_obj_t *sub;

    screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x102030), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    title = lv_label_create(screen);
    lv_label_set_text(title, "GD32C231C");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    sub = lv_label_create(screen);
    lv_label_set_text(sub, "FreeRTOS + LVGL");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x9AD7FF), 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 58);

    temp_label = lv_label_create(screen);
    lv_label_set_text(temp_label, "Temp: --.- C");
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, -28);

    humi_label = lv_label_create(screen);
    lv_label_set_text(humi_label, "Humi: --.- %");
    lv_obj_set_style_text_color(humi_label, lv_color_hex(0xD8F3DC), 0);
    lv_obj_align(humi_label, LV_ALIGN_CENTER, 0, 0);

    state_label = lv_label_create(screen);
    lv_label_set_text(state_label, "DHT11 PB0: WAIT");
    lv_obj_set_style_text_color(state_label, lv_color_hex(0xF9D65C), 0);
    lv_obj_align(state_label, LV_ALIGN_CENTER, 0, 32);

    count_label = lv_label_create(screen);
    lv_label_set_text(count_label, "Sample: 0  Error: 0");
    lv_obj_set_style_text_color(count_label, lv_color_hex(0x9AD7FF), 0);
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, -32);
}

static void update_demo_screen(void)
{
    sensor_state_t snapshot;
    uint16_t temperature_x10;
    uint16_t humidity_x10;

    taskENTER_CRITICAL();
    snapshot = sensor_state;
    taskEXIT_CRITICAL();

    if(DHT11_OK == snapshot.status) {
        temperature_x10 = ((uint16_t)snapshot.data.temperature_int * 10U) + snapshot.data.temperature_dec;
        humidity_x10 = ((uint16_t)snapshot.data.humidity_int * 10U) + snapshot.data.humidity_dec;

        lv_label_set_text_fmt(temp_label, "Temp: %u.%u C", temperature_x10 / 10U, temperature_x10 % 10U);
        lv_label_set_text_fmt(humi_label, "Humi: %u.%u %%", humidity_x10 / 10U, humidity_x10 % 10U);
        lv_label_set_text(state_label, "DHT11 PB0: OK");
        lv_obj_set_style_text_color(state_label, lv_color_hex(0x33D17A), 0);
    } else {
        lv_label_set_text(temp_label, "Temp: --.- C");
        lv_label_set_text(humi_label, "Humi: --.- %");
        lv_label_set_text_fmt(state_label, "DHT11 PB0: %s", dht11_status_string(snapshot.status));
        lv_obj_set_style_text_color(state_label, lv_color_hex(0xFF6B6B), 0);
    }

    lv_label_set_text_fmt(count_label,
                          "Sample: %lu  Error: %lu",
                          (unsigned long)snapshot.sample_count,
                          (unsigned long)snapshot.error_count);
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    while(1) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    taskDISABLE_INTERRUPTS();
    while(1) {
    }
}

void vApplicationIdleHook(void)
{
}
