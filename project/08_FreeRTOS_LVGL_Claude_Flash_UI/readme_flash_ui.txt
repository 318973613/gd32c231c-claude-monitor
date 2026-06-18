GD32C231C-EVAL FreeRTOS LVGL Claude Flash UI

Purpose:
  This project reads the UI background and text assets from the on-board
  GD25Q16 external SPI flash, then runs the FreeRTOS + LVGL + DHT11 dashboard.

Keil project:
  MDK-ARM\GD32C231C_FreeRTOS_Claude_Flash_UI.uvprojx

Output:
  MDK-ARM\Objects\FreeRTOS_Claude_Flash_UI.hex

Required first step:
  Download and run stage 07 once:
    project\07_SPI_FLASH_AssetWriter\MDK-ARM\Objects\SPI_FLASH_AssetWriter.hex

Runtime:
  1. app_flash_assets reads and verifies the CUI1 asset package from GD25Q16.
  2. The 240x320 RGB565 RLE background is decoded line by line.
  3. LCD and Flash share SPI1, so the code switches SPI mode between GD25Q16
     access and LCD drawing.
  4. LVGL overlays dynamic labels: clock, DHT11 status, sample count and RX count.
  5. USART0 still sets the software clock:
       TIME=12:34:56
       T=08:00:00

Build result:
  Program Size: Code=59862 RO-data=2370 RW-data=156 ZI-data=10836
  Build status: 0 Error(s), 0 Warning(s)
