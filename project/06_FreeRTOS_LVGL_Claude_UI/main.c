/*!
    \file    main.c
    \brief   FreeRTOS + LVGL Claude style dashboard for GD32C231C-EVAL

    \version 2026-06-17, custom LVGL demo for gd32c2x1
*/

#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "dht11.h"
#include "app_dashboard.h"
#include "app_led_effect.h"
#include "app_serial_clock.h"
#include "app_status.h"

#define GUI_TASK_STACK_WORDS     768U
#define GUI_TASK_PRIORITY        (tskIDLE_PRIORITY + 2U)
#define DHT_TASK_STACK_WORDS     128U
#define DHT_TASK_PRIORITY        (tskIDLE_PRIORITY + 1U)

#define DHT_SAMPLE_PERIOD_MS     2000U
#define GUI_UPDATE_PERIOD_MS     250U

static app_sensor_state_t sensor_state = {{0U, 0U, 0U, 0U}, DHT11_ERROR_TIMEOUT, 0U, 0U};

static void gui_task(void *argument);
static void dht11_task(void *argument);

int main(void)
{
    app_led_effect_init();
    app_serial_clock_init();

    if(pdPASS != xTaskCreate(gui_task, "gui", GUI_TASK_STACK_WORDS, NULL, GUI_TASK_PRIORITY, NULL)) {
        app_led_effect_error();
        while(1) {
        }
    }

    if(pdPASS != xTaskCreate(dht11_task, "dht11", DHT_TASK_STACK_WORDS, NULL, DHT_TASK_PRIORITY, NULL)) {
        app_led_effect_error();
        while(1) {
        }
    }

    vTaskStartScheduler();

    app_led_effect_error();
    while(1) {
    }
}

static void gui_task(void *argument)
{
    TickType_t last_update;
    TickType_t now;
    app_sensor_state_t sensor_snapshot;
    app_clock_state_t clock_snapshot;
    uint8_t rx_event;

    (void)argument;

    app_dashboard_init();
    last_update = xTaskGetTickCount();

    while(1) {
        now = xTaskGetTickCount();

        lv_timer_handler();
        rx_event = app_serial_clock_process();
        app_serial_clock_update(now);
        app_serial_clock_get_state(&clock_snapshot);

        taskENTER_CRITICAL();
        sensor_snapshot = sensor_state;
        taskEXIT_CRITICAL();

        app_led_effect_update(now,
                              clock_snapshot.valid,
                              (DHT11_OK == sensor_snapshot.status) ? 1U : 0U,
                              rx_event);

        if((now - last_update) >= pdMS_TO_TICKS(GUI_UPDATE_PERIOD_MS)) {
            last_update = now;
            app_dashboard_update(&sensor_snapshot, &clock_snapshot);
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

void vApplicationMallocFailedHook(void)
{
    app_led_effect_fault(0x0AU);
    taskDISABLE_INTERRUPTS();
    while(1) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    (void)task_name;
    app_led_effect_fault(0x05U);
    taskDISABLE_INTERRUPTS();
    while(1) {
    }
}

void vApplicationIdleHook(void)
{
}
