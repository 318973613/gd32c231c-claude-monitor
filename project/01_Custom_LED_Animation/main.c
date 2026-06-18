/*!
    \file    main.c
    \brief   custom four-led animation

    \version 2026-02-06, V1.2.0, demo for gd32c2x1
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "gd32c2x1.h"
#include "systick.h"

/* LED bit order: bit0=LED1(PA15), bit1=LED2(PD1), bit2=LED3(PD3), bit3=LED4(PB4). */
#define LED_COUNT        4U
#define LED_STEP_MS      140U
#define LED_HOLD_MS      260U

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

static void led_gpio_config(void);
static void led_write_mask(uint8_t mask);
static void led_play_pattern(const uint8_t pattern[], uint32_t length, uint32_t delay_ms);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/

int main(void)
{
    uint8_t mask;
    const uint8_t boot_pattern[] = {0x0FU, 0x00U, 0x0FU, 0x00U, 0x0FU, 0x00U};
    const uint8_t fill_pattern[] = {0x01U, 0x03U, 0x07U, 0x0FU, 0x0EU, 0x0CU, 0x08U, 0x00U};
    const uint8_t mirror_pattern[] = {0x09U, 0x06U, 0x09U, 0x06U, 0x0FU, 0x00U};

    systick_config();
    led_gpio_config();

    led_play_pattern(boot_pattern, sizeof(boot_pattern) / sizeof(boot_pattern[0]), LED_HOLD_MS);

    while(1) {
        led_play_pattern(fill_pattern, sizeof(fill_pattern) / sizeof(fill_pattern[0]), LED_STEP_MS);
        led_play_pattern(mirror_pattern, sizeof(mirror_pattern) / sizeof(mirror_pattern[0]), LED_HOLD_MS);

        /* 4-bit counter demo: all LED combinations from 0000 to 1111. */
        for(mask = 0U; mask < 16U; mask++) {
            led_write_mask(mask);
            delay_1ms(LED_STEP_MS);
        }
    }
}

/*!
    \brief      configure all four LED GPIO pins
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void led_gpio_config(void)
{
    uint32_t i;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOB);

    for(i = 0U; i < LED_COUNT; i++) {
        gpio_mode_set(led_port[i], GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, led_pin[i]);
        gpio_output_options_set(led_port[i], GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL_1, led_pin[i]);
    }

    led_write_mask(0x00U);
}

/*!
    \brief      set LED state by bit mask
    \param[in]  mask: bit0 LED1, bit1 LED2, bit2 LED3, bit3 LED4
    \param[out] none
    \retval     none
*/
static void led_write_mask(uint8_t mask)
{
    uint32_t i;

    for(i = 0U; i < LED_COUNT; i++) {
        if(0U != (mask & (uint8_t)(1U << i))) {
            gpio_bit_set(led_port[i], led_pin[i]);
        } else {
            gpio_bit_reset(led_port[i], led_pin[i]);
        }
    }
}

/*!
    \brief      play a LED mask sequence
    \param[in]  pattern: LED mask sequence
    \param[in]  length: pattern length
    \param[in]  delay_ms: delay after each mask
    \param[out] none
    \retval     none
*/
static void led_play_pattern(const uint8_t pattern[], uint32_t length, uint32_t delay_ms)
{
    uint32_t i;

    for(i = 0U; i < length; i++) {
        led_write_mask(pattern[i]);
        delay_1ms(delay_ms);
    }
}
