#ifndef APP_SERIAL_CLOCK_H
#define APP_SERIAL_CLOCK_H

#include "FreeRTOS.h"
#include "task.h"
#include "app_status.h"
#include <stdint.h>

void app_serial_clock_init(void);
void app_serial_clock_update(TickType_t now);
uint8_t app_serial_clock_process(void);
void app_serial_clock_get_state(app_clock_state_t *state);

#endif /* APP_SERIAL_CLOCK_H */
