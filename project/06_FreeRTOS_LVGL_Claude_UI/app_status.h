#ifndef APP_STATUS_H
#define APP_STATUS_H

#include "dht11.h"
#include <stdint.h>

typedef struct {
    dht11_data_t data;
    dht11_status_t status;
    uint32_t sample_count;
    uint32_t error_count;
} app_sensor_state_t;

typedef struct {
    uint32_t seconds;
    uint8_t valid;
    uint32_t set_count;
    uint32_t rx_count;
    uint32_t overflow_count;
} app_clock_state_t;

#endif /* APP_STATUS_H */
