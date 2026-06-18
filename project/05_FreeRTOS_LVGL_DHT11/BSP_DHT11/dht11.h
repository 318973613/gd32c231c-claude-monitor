#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>

typedef enum {
    DHT11_OK = 0,
    DHT11_ERROR_TIMEOUT,
    DHT11_ERROR_CHECKSUM,
    DHT11_ERROR_ZERO_DATA,
    DHT11_ERROR_ARGUMENT
} dht11_status_t;

typedef struct {
    uint8_t humidity_int;
    uint8_t humidity_dec;
    uint8_t temperature_int;
    uint8_t temperature_dec;
} dht11_data_t;

void dht11_init(void);
dht11_status_t dht11_read(dht11_data_t *data);
const char *dht11_status_string(dht11_status_t status);

#endif /* DHT11_H */
