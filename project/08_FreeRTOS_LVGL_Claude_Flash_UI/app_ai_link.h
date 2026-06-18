#ifndef APP_AI_LINK_H
#define APP_AI_LINK_H

#include "FreeRTOS.h"
#include "task.h"
#include "app_status.h"
#include <stdint.h>

void app_ai_link_init(void);
uint8_t app_ai_link_process_line(const char *line);
uint8_t app_ai_link_update_buttons(TickType_t now);
void app_ai_link_get_state(app_ai_state_t *state);

#endif /* APP_AI_LINK_H */
