#include "app_led_effect.h"
#include "gd32c2x1.h"

#define LED_COUNT                 4U
#define LED_EFFECT_PERIOD_MS      90U
#define RX_SPARK_FRAMES           8U

static const uint32_t led_port[LED_COUNT] = {
    GPIOA,
    GPIOD,
    GPIOD,
    GPIOB
};

static const uint32_t led_pin[LED_COUNT] = {
    GPIO_PIN_15,
    GPIO_PIN_1,
    GPIO_PIN_3,
    GPIO_PIN_4
};

static const uint8_t flow_pattern[] = {
    0x01U, 0x03U, 0x02U, 0x06U, 0x04U, 0x0CU,
    0x08U, 0x0CU, 0x04U, 0x06U, 0x02U, 0x03U
};

static uint8_t rx_spark_frames = 0U;

static void led_write(uint32_t index, uint8_t on);
static void led_mask_write(uint8_t mask);

void app_led_effect_init(void)
{
    uint32_t i;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOB);

    for(i = 0U; i < LED_COUNT; i++) {
        gpio_mode_set(led_port[i], GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, led_pin[i]);
        gpio_output_options_set(led_port[i], GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, led_pin[i]);
    }

    led_mask_write(0x00U);
}

void app_led_effect_update(TickType_t now, uint8_t clock_valid, uint8_t dht_ok, uint8_t rx_event)
{
    static TickType_t last_tick = 0U;
    static uint8_t pattern_index = 0U;
    uint8_t mask;

    if(0U != rx_event) {
        rx_spark_frames = RX_SPARK_FRAMES;
    }

    if((now - last_tick) < pdMS_TO_TICKS(LED_EFFECT_PERIOD_MS)) {
        return;
    }
    last_tick = now;

    if(rx_spark_frames > 0U) {
        rx_spark_frames--;
        mask = (0U != (rx_spark_frames & 0x01U)) ? 0x0FU : 0x05U;
    } else if(0U == clock_valid) {
        mask = (0U != (pattern_index & 0x01U)) ? 0x09U : 0x06U;
        pattern_index++;
    } else if(0U == dht_ok) {
        mask = (0U != (pattern_index & 0x01U)) ? 0x0FU : 0x00U;
        pattern_index++;
    } else {
        mask = flow_pattern[pattern_index];
        pattern_index++;
        if(pattern_index >= (sizeof(flow_pattern) / sizeof(flow_pattern[0]))) {
            pattern_index = 0U;
        }
    }

    led_mask_write(mask);
}

void app_led_effect_error(void)
{
    led_mask_write(0x0FU);
}

void app_led_effect_fault(uint8_t mask)
{
    led_mask_write(mask);
}

static void led_write(uint32_t index, uint8_t on)
{
    if(index >= LED_COUNT) {
        return;
    }

    if(0U != on) {
        gpio_bit_set(led_port[index], led_pin[index]);
    } else {
        gpio_bit_reset(led_port[index], led_pin[index]);
    }
}

static void led_mask_write(uint8_t mask)
{
    uint32_t i;

    for(i = 0U; i < LED_COUNT; i++) {
        led_write(i, (uint8_t)((mask >> i) & 0x01U));
    }
}
