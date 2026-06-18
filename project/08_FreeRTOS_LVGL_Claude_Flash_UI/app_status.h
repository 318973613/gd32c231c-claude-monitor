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

typedef enum {
    APP_AI_IDLE = 0,
    APP_AI_WORKING,
    APP_AI_NATIVE_APPROVE,
    APP_AI_WAIT_APPROVE,
    APP_AI_APPROVED,
    APP_AI_DENIED,
    APP_AI_ERROR
} app_ai_mode_t;

typedef struct {
    app_ai_mode_t mode;
    uint32_t event_count;
    uint32_t approve_count;
    uint32_t deny_count;
    uint32_t last_change_ms;
    char model[16];
    char tool[12];
    uint8_t context_used_percent;
    uint8_t context_remaining_percent;
    uint8_t limit_used_percent;
    uint8_t limit_remaining_percent;
    uint16_t cost_usd_cents;
    uint8_t has_context;
    uint8_t has_limit;
    uint8_t has_cost;
} app_ai_state_t;

#endif /* APP_STATUS_H */
