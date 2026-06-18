#ifndef APP_LED_EFFECT_H
#define APP_LED_EFFECT_H

#include "FreeRTOS.h"
#include "task.h"
#include "app_status.h"
#include <stdint.h>

void app_led_effect_init(void);
void app_led_effect_update(TickType_t now,
                           uint8_t clock_valid,
                           uint8_t dht_ok,
                           uint8_t rx_event,
                           app_ai_mode_t ai_mode);
void app_led_effect_error(void);
void app_led_effect_fault(uint8_t mask);

#endif /* APP_LED_EFFECT_H */
