/*!
    \file    main.c
    \brief   GD25Q16 external flash asset writer for GD32C231C-EVAL

    \version 2026-06-17, custom external asset writer
*/

#include "gd32c2x1.h"
#include "systick.h"
#include "gd25qxx.h"
#include "gd32c231c_eval.h"
#include "ui_assets.h"
#include <stdio.h>

#define SFLASH_ID                 0xC84015U
#define ASSET_FLASH_ADDRESS       0x000000U
#define SECTOR_SIZE               4096U
#define WRITE_CHUNK_SIZE          256U

static uint8_t verify_buffer[WRITE_CHUNK_SIZE];

static void led_init_all(void);
static void led_error_loop(void);
static uint8_t write_asset_blob(void);
static uint8_t verify_asset_blob(void);

int main(void)
{
    uint32_t flash_id;

    systick_config();
    led_init_all();
    gd_eval_com_init(EVAL_COM);
    spi_flash_init();

    printf("\r\n[07] GD32C231C external flash asset writer\r\n");
    printf("Asset package size: %lu bytes\r\n", (unsigned long)ui_asset_blob_size);

    flash_id = spi_flash_read_id();
    printf("GD25Qxx ID: 0x%06lX\r\n", (unsigned long)flash_id);

    if(SFLASH_ID != flash_id) {
        printf("ERR: expected GD25Q16 ID 0x%06lX\r\n", (unsigned long)SFLASH_ID);
        led_error_loop();
    }

    printf("Writing Claude UI asset package...\r\n");
    if(0U == write_asset_blob()) {
        printf("ERR: write failed\r\n");
        led_error_loop();
    }

    printf("Verifying...\r\n");
    if(0U == verify_asset_blob()) {
        printf("ERR: verify failed\r\n");
        led_error_loop();
    }

    printf("OK: assets stored at external flash 0x%06lX\r\n", (unsigned long)ASSET_FLASH_ADDRESS);

    while(1) {
        gd_eval_led_on(LED1);
        gd_eval_led_on(LED2);
        gd_eval_led_on(LED3);
        gd_eval_led_on(LED4);
        delay_1ms(120);
        gd_eval_led_off(LED1);
        gd_eval_led_off(LED2);
        gd_eval_led_off(LED3);
        gd_eval_led_off(LED4);
        delay_1ms(380);
    }
}

static void led_init_all(void)
{
    gd_eval_led_init(LED1);
    gd_eval_led_init(LED2);
    gd_eval_led_init(LED3);
    gd_eval_led_init(LED4);
    gd_eval_led_off(LED1);
    gd_eval_led_off(LED2);
    gd_eval_led_off(LED3);
    gd_eval_led_off(LED4);
}

static void led_error_loop(void)
{
    while(1) {
        gd_eval_led_on(LED1);
        gd_eval_led_off(LED2);
        gd_eval_led_on(LED3);
        gd_eval_led_off(LED4);
        delay_1ms(150);
        gd_eval_led_off(LED1);
        gd_eval_led_on(LED2);
        gd_eval_led_off(LED3);
        gd_eval_led_on(LED4);
        delay_1ms(150);
    }
}

static uint8_t write_asset_blob(void)
{
    uint32_t erase_addr;
    uint32_t end_addr;
    uint32_t written = 0U;
    uint32_t chunk;

    end_addr = ASSET_FLASH_ADDRESS + ui_asset_blob_size;
    for(erase_addr = ASSET_FLASH_ADDRESS; erase_addr < end_addr; erase_addr += SECTOR_SIZE) {
        spi_flash_sector_erase(erase_addr);
    }

    while(written < ui_asset_blob_size) {
        chunk = ui_asset_blob_size - written;
        if(chunk > WRITE_CHUNK_SIZE) {
            chunk = WRITE_CHUNK_SIZE;
        }

        spi_flash_buffer_write((uint8_t *)&ui_asset_blob[written],
                               ASSET_FLASH_ADDRESS + written,
                               (uint16_t)chunk);
        written += chunk;
    }

    return 1U;
}

static uint8_t verify_asset_blob(void)
{
    uint32_t checked = 0U;
    uint32_t chunk;
    uint32_t i;

    while(checked < ui_asset_blob_size) {
        chunk = ui_asset_blob_size - checked;
        if(chunk > WRITE_CHUNK_SIZE) {
            chunk = WRITE_CHUNK_SIZE;
        }

        spi_flash_buffer_read(verify_buffer, ASSET_FLASH_ADDRESS + checked, (uint16_t)chunk);
        for(i = 0U; i < chunk; i++) {
            if(verify_buffer[i] != ui_asset_blob[checked + i]) {
                printf("Mismatch at 0x%06lX: flash=0x%02X asset=0x%02X\r\n",
                       (unsigned long)(ASSET_FLASH_ADDRESS + checked + i),
                       verify_buffer[i],
                       ui_asset_blob[checked + i]);
                return 0U;
            }
        }

        checked += chunk;
    }

    return 1U;
}
