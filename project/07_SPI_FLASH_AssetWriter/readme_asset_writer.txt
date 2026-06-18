GD32C231C-EVAL External Flash Asset Writer

Purpose:
  This project writes the Claude UI asset package into the on-board GD25Q16
  SPI flash.

Keil project:
  MDK-ARM\GD32C231C_SPI_FLASH_AssetWriter.uvprojx

Output:
  MDK-ARM\Objects\SPI_FLASH_AssetWriter.hex

External flash:
  Device: GD25Q16
  JEDEC ID: 0xC84015
  SPI: SPI1
  SCK : PB13
  MISO: PB14
  MOSI: PB15
  CS  : PB11

Asset package:
  Address: 0x000000
  Format : CUI1 custom package
  Data   : 240x320 RGB565 RLE background + ASCII UI text strings

How to use:
  1. Download SPI_FLASH_AssetWriter.hex.
  2. Open USART0 PA9/PA10 at 115200 8N1.
  3. Wait for "OK: assets stored at external flash".
  4. After success, all four LEDs blink together.
  5. Then download the stage 08 application project.

Build result:
  Program Size: Code=9472 RO-data=14072 RW-data=8 ZI-data=1288
  Build status: 0 Error(s), 0 Warning(s)
