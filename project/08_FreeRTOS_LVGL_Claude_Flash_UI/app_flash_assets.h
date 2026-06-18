#ifndef APP_FLASH_ASSETS_H
#define APP_FLASH_ASSETS_H

#include <stdint.h>

#define APP_ASSET_TEXT_MAX        32U
#define APP_ASSET_ID_BACKGROUND   1U
#define APP_ASSET_ID_TITLE        2U
#define APP_ASSET_ID_HINT         3U
#define APP_ASSET_ID_FOOTER       4U

typedef enum {
    APP_ASSET_OK = 0,
    APP_ASSET_FLASH_ID_ERROR,
    APP_ASSET_HEADER_ERROR,
    APP_ASSET_CHECKSUM_ERROR,
    APP_ASSET_NOT_FOUND
} app_asset_status_t;

app_asset_status_t app_flash_assets_init(void);
app_asset_status_t app_flash_assets_draw_background(void);
app_asset_status_t app_flash_assets_read_text(uint16_t asset_id, char *text, uint16_t max_len);

#endif /* APP_FLASH_ASSETS_H */
