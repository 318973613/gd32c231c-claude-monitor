#ifndef APP_DASHBOARD_H
#define APP_DASHBOARD_H

#include "app_status.h"

void app_dashboard_init(void);
void app_dashboard_update(const app_sensor_state_t *sensor, const app_clock_state_t *clock);

#endif /* APP_DASHBOARD_H */
