#include "dht11.h"
#include "gd32c2x1.h"
#include "FreeRTOS.h"
#include "task.h"

#define DHT11_RCU          RCU_GPIOB
#define DHT11_PORT         GPIOB
#define DHT11_PIN          GPIO_PIN_0
#define DHT11_START_MS     20U
#define DHT11_RELEASE_US   30U
#define DHT11_TIMEOUT_US   120U
#define DHT11_BIT_SAMPLE_US 40U

static void dht11_delay_us(uint32_t us);
static void dht11_drive_low(void);
static void dht11_release_line(void);
static uint8_t dht11_read_pin(void);
static dht11_status_t dht11_wait_level(uint8_t level, uint32_t timeout_us, uint32_t *elapsed_us);

void dht11_init(void)
{
    rcu_periph_clock_enable(DHT11_RCU);
    dht11_release_line();
}

dht11_status_t dht11_read(dht11_data_t *data)
{
    uint8_t raw[5] = {0U, 0U, 0U, 0U, 0U};
    uint32_t bit;
    dht11_status_t status;

    if(NULL == data) {
        return DHT11_ERROR_ARGUMENT;
    }

    dht11_drive_low();
    vTaskDelay(pdMS_TO_TICKS(DHT11_START_MS));

    taskENTER_CRITICAL();

    dht11_release_line();
    dht11_delay_us(DHT11_RELEASE_US);

    status = dht11_wait_level(1U, DHT11_TIMEOUT_US, NULL);
    if(DHT11_OK != status) {
        taskEXIT_CRITICAL();
        dht11_release_line();
        return status;
    }

    status = dht11_wait_level(0U, DHT11_TIMEOUT_US, NULL);
    if(DHT11_OK != status) {
        taskEXIT_CRITICAL();
        dht11_release_line();
        return status;
    }

    status = dht11_wait_level(1U, DHT11_TIMEOUT_US, NULL);
    if(DHT11_OK != status) {
        taskEXIT_CRITICAL();
        dht11_release_line();
        return status;
    }

    for(bit = 0U; bit < 40U; bit++) {
        status = dht11_wait_level(0U, DHT11_TIMEOUT_US, NULL);
        if(DHT11_OK != status) {
            taskEXIT_CRITICAL();
            dht11_release_line();
            return status;
        }

        dht11_delay_us(DHT11_BIT_SAMPLE_US);

        raw[bit / 8U] <<= 1U;
        if(0U != dht11_read_pin()) {
            raw[bit / 8U] |= 1U;
        }

        status = dht11_wait_level(1U, DHT11_TIMEOUT_US, NULL);
        if(DHT11_OK != status) {
            taskEXIT_CRITICAL();
            dht11_release_line();
            return status;
        }
    }

    taskEXIT_CRITICAL();
    dht11_release_line();

    if((uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]) != raw[4]) {
        return DHT11_ERROR_CHECKSUM;
    }

    if((0U == raw[0]) && (0U == raw[1]) && (0U == raw[2]) && (0U == raw[3]) && (0U == raw[4])) {
        return DHT11_ERROR_ZERO_DATA;
    }

    data->humidity_int = raw[0];
    data->humidity_dec = raw[1];
    data->temperature_int = raw[2];
    data->temperature_dec = raw[3];

    return DHT11_OK;
}

const char *dht11_status_string(dht11_status_t status)
{
    switch(status) {
    case DHT11_OK:
        return "OK";
    case DHT11_ERROR_TIMEOUT:
        return "TIMEOUT";
    case DHT11_ERROR_CHECKSUM:
        return "CHECKSUM";
    case DHT11_ERROR_ZERO_DATA:
        return "ZERO";
    case DHT11_ERROR_ARGUMENT:
        return "ARG";
    default:
        return "UNKNOWN";
    }
}

static void dht11_delay_us(uint32_t us)
{
    uint32_t ticks_per_us;
    uint32_t target_ticks;
    uint32_t elapsed_ticks = 0U;
    uint32_t reload;
    uint32_t last;
    uint32_t now;

    ticks_per_us = SystemCoreClock / 1000000U;
    if(0U == ticks_per_us) {
        ticks_per_us = 1U;
    }

    target_ticks = us * ticks_per_us;
    reload = SysTick->LOAD;
    last = SysTick->VAL;

    while(elapsed_ticks < target_ticks) {
        now = SysTick->VAL;
        if(last >= now) {
            elapsed_ticks += last - now;
        } else {
            elapsed_ticks += last + (reload - now) + 1U;
        }
        last = now;
    }
}

static void dht11_drive_low(void)
{
    gpio_mode_set(DHT11_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DHT11_PIN);
    gpio_output_options_set(DHT11_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_LEVEL_1, DHT11_PIN);
    GPIO_BC(DHT11_PORT) = DHT11_PIN;
}

static void dht11_release_line(void)
{
    gpio_mode_set(DHT11_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, DHT11_PIN);
}

static uint8_t dht11_read_pin(void)
{
    return (RESET != gpio_input_bit_get(DHT11_PORT, DHT11_PIN)) ? 1U : 0U;
}

static dht11_status_t dht11_wait_level(uint8_t level, uint32_t timeout_us, uint32_t *elapsed_us)
{
    uint32_t elapsed = 0U;

    while(dht11_read_pin() == level) {
        if(elapsed >= timeout_us) {
            if(NULL != elapsed_us) {
                *elapsed_us = elapsed;
            }
            return DHT11_ERROR_TIMEOUT;
        }

        dht11_delay_us(1U);
        elapsed++;
    }

    if(NULL != elapsed_us) {
        *elapsed_us = elapsed;
    }

    return DHT11_OK;
}
