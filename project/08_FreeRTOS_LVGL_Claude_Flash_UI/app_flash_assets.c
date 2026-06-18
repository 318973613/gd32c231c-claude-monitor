#include "app_flash_assets.h"
#include "gd25qxx.h"
#include "lcd_driver.h"
#include <string.h>

#define SFLASH_ID                 0xC84015U
#define ASSET_BASE_ADDRESS        0x000000U
#define ASSET_MAGIC               0x31495543U
#define ASSET_TYPE_RLE_RGB565     1U
#define ASSET_TYPE_TEXT           2U
#define ASSET_ID_BACKGROUND       1U
#define LCD_W                     240U
#define LCD_H                     320U
#define MAX_ASSETS                8U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t total_size;
    uint16_t asset_count;
    uint16_t reserved;
    uint32_t checksum;
} asset_header_t;

typedef struct {
    uint16_t id;
    uint16_t type;
    uint32_t offset;
    uint32_t size;
    uint16_t width;
    uint16_t height;
    uint16_t format;
    uint16_t reserved;
} asset_entry_t;

static asset_header_t header;
static asset_entry_t entries[MAX_ASSETS];
static uint16_t line_buffer[LCD_W];
static uint8_t assets_ready = 0U;

static uint16_t read_le16(const uint8_t *p);
static uint32_t read_le32(const uint8_t *p);
static void load_header(const uint8_t *raw);
static void load_entry(asset_entry_t *entry, const uint8_t *raw);
static app_asset_status_t find_asset(uint16_t asset_id, asset_entry_t *entry);
static uint32_t calculate_checksum(uint32_t start, uint32_t length);

app_asset_status_t app_flash_assets_init(void)
{
    uint8_t raw_header[20];
    uint8_t raw_entry[20];
    uint16_t i;

    spi_flash_init();

    if(SFLASH_ID != spi_flash_read_id()) {
        return APP_ASSET_FLASH_ID_ERROR;
    }

    spi_flash_buffer_read(raw_header, ASSET_BASE_ADDRESS, sizeof(raw_header));
    load_header(raw_header);

    if((ASSET_MAGIC != header.magic) ||
       (1U != header.version) ||
       (header.asset_count > MAX_ASSETS) ||
       (header.header_size < sizeof(raw_header)) ||
       (header.total_size <= header.header_size)) {
        return APP_ASSET_HEADER_ERROR;
    }

    for(i = 0U; i < header.asset_count; i++) {
        spi_flash_buffer_read(raw_entry,
                              ASSET_BASE_ADDRESS + 20U + ((uint32_t)i * 20U),
                              sizeof(raw_entry));
        load_entry(&entries[i], raw_entry);
    }

    if(header.checksum != calculate_checksum(ASSET_BASE_ADDRESS + 20U, header.total_size - 20U)) {
        return APP_ASSET_CHECKSUM_ERROR;
    }

    assets_ready = 1U;
    return APP_ASSET_OK;
}

app_asset_status_t app_flash_assets_draw_background(void)
{
    asset_entry_t bg;
    uint32_t addr;
    uint32_t end;
    uint32_t line_index = 0U;
    uint32_t x = 0U;
    uint16_t run;
    uint16_t color;
    uint16_t take;
    uint8_t raw_run[4];
    app_asset_status_t status;

    status = find_asset(ASSET_ID_BACKGROUND, &bg);
    if(APP_ASSET_OK != status) {
        return status;
    }

    if((ASSET_TYPE_RLE_RGB565 != bg.type) || (LCD_W != bg.width) || (LCD_H != bg.height)) {
        return APP_ASSET_HEADER_ERROR;
    }

    addr = ASSET_BASE_ADDRESS + bg.offset;
    end = addr + bg.size;
    memset(line_buffer, 0, sizeof(line_buffer));

    spi_flash_init();
    while(addr < end && line_index < LCD_H) {
        spi_flash_buffer_read(raw_run, addr, sizeof(raw_run));
        addr += sizeof(raw_run);
        run = read_le16(raw_run);
        color = read_le16(&raw_run[2]);

        while((run > 0U) && (line_index < LCD_H)) {
            take = (uint16_t)(LCD_W - x);
            if(take > run) {
                take = run;
            }

            while(take > 0U) {
                line_buffer[x] = color;
                x++;
                run--;
                take--;
            }

            if(x >= LCD_W) {
                lcd_bus_init();
                lcd_flush_area(0U, (uint16_t)line_index, LCD_W - 1U, (uint16_t)line_index, line_buffer);
                line_index++;
                x = 0U;
                if((addr < end) && (line_index < LCD_H)) {
                    spi_flash_init();
                }
            }
        }
    }

    lcd_bus_init();
    return (line_index == LCD_H) ? APP_ASSET_OK : APP_ASSET_HEADER_ERROR;
}

app_asset_status_t app_flash_assets_read_text(uint16_t asset_id, char *text, uint16_t max_len)
{
    asset_entry_t entry;
    uint16_t read_len;
    app_asset_status_t status;

    if((NULL == text) || (0U == max_len)) {
        return APP_ASSET_HEADER_ERROR;
    }

    text[0] = '\0';
    status = find_asset(asset_id, &entry);
    if(APP_ASSET_OK != status) {
        return status;
    }

    if(ASSET_TYPE_TEXT != entry.type) {
        return APP_ASSET_HEADER_ERROR;
    }

    read_len = (entry.size < (uint32_t)(max_len - 1U)) ? (uint16_t)entry.size : (uint16_t)(max_len - 1U);
    spi_flash_init();
    spi_flash_buffer_read((uint8_t *)text, ASSET_BASE_ADDRESS + entry.offset, read_len);
    text[read_len] = '\0';
    lcd_bus_init();
    return APP_ASSET_OK;
}

static app_asset_status_t find_asset(uint16_t asset_id, asset_entry_t *entry)
{
    uint16_t i;

    if((0U == assets_ready) || (NULL == entry)) {
        return APP_ASSET_HEADER_ERROR;
    }

    for(i = 0U; i < header.asset_count; i++) {
        if(entries[i].id == asset_id) {
            *entry = entries[i];
            return APP_ASSET_OK;
        }
    }

    return APP_ASSET_NOT_FOUND;
}

static uint32_t calculate_checksum(uint32_t start, uint32_t length)
{
    uint8_t buffer[64];
    uint32_t offset = 0U;
    uint32_t chunk;
    uint32_t i;
    uint32_t sum = 0U;

    while(offset < length) {
        chunk = length - offset;
        if(chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }

        spi_flash_buffer_read(buffer, start + offset, (uint16_t)chunk);
        for(i = 0U; i < chunk; i++) {
            sum += buffer[i];
        }
        offset += chunk;
    }

    return sum;
}

static void load_header(const uint8_t *raw)
{
    header.magic = read_le32(raw);
    header.version = read_le16(&raw[4]);
    header.header_size = read_le16(&raw[6]);
    header.total_size = read_le32(&raw[8]);
    header.asset_count = read_le16(&raw[12]);
    header.reserved = read_le16(&raw[14]);
    header.checksum = read_le32(&raw[16]);
}

static void load_entry(asset_entry_t *entry, const uint8_t *raw)
{
    entry->id = read_le16(raw);
    entry->type = read_le16(&raw[2]);
    entry->offset = read_le32(&raw[4]);
    entry->size = read_le32(&raw[8]);
    entry->width = read_le16(&raw[12]);
    entry->height = read_le16(&raw[14]);
    entry->format = read_le16(&raw[16]);
    entry->reserved = read_le16(&raw[18]);
}

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
