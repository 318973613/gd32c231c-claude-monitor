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

#define LCD_HOR_RES              240U
#define LCD_VER_RES              320U
#define LVGL_BUFFER_LINES        2U
#define GUI_TASK_STACK_WORDS     768U
#define GUI_TASK_PRIORITY        (tskIDLE_PRIORITY + 2U)

static lv_disp_draw_buf_t draw_buf;
static lv_color_t lvgl_buf[LCD_HOR_RES * LVGL_BUFFER_LINES];

static void gui_task(void *argument);
static void lvgl_port_init(void);
static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void create_demo_screen(void);

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
    (void)argument;

    lcd_init();
    lcd_clear(BLACK);
    lv_init();
    lvgl_port_init();
    create_demo_screen();

    while(1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5U));
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
    lv_obj_t *status;

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

    status = lv_label_create(screen);
    lv_label_set_text(status, "LCD refresh task running");
    lv_obj_set_style_text_color(status, lv_color_hex(0xD8F3DC), 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 8);
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
