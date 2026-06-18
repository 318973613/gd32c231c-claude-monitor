GD32C231C-EVAL FreeRTOS LVGL display

This is the fourth evaluation stage project. It keeps the FreeRTOS base and
ports LVGL to the SPI LCD only. USART and LED logic are intentionally removed
from this stage.

Target board:
  GD32C231C-EVAL

Target device:
  GD32C231C8, Cortex-M23
  Flash: 64 KB
  SRAM : 12 KB

Keil project:
  MDK-ARM\GD32C231C_FreeRTOS_LVGL.uvprojx

Dependencies:
  ..\GD32C2x1_Firmware_Library
  LVGL source copied from:
    ..\..\third_party\lvgl-v8.4.0

LVGL version:
  v8.4.0

Why LVGL v8.4.0:
  GD32C231C8 has a small 64 KB Flash and 12 KB SRAM budget. LVGL v8 is easier
  to cut down for this MCU than LVGL v9.

LCD hardware:
  Controller style: ILI9341-compatible SPI LCD
  Resolution      : 240 x 320
  SPI             : SPI1
  SCK/MISO/MOSI   : PB13 / PB14 / PB15
  LCD_CS          : PA8
  LCD_DC          : PC7
  LCD_RST         : PC6

Runtime design:
  One FreeRTOS GUI task initializes LCD, initializes LVGL, registers the LVGL
  display driver, creates a simple screen, and periodically calls
  lv_timer_handler().

LVGL tick:
  LV_TICK_CUSTOM is enabled.
  lv_tick_get() uses xTaskGetTickCount() through lv_port_tick.h.
  FreeRTOS itself still uses the Cortex-M SysTick port tick.

LVGL display flush:
  main.c registers lvgl_flush_cb().
  lvgl_flush_cb() calls lcd_flush_area().
  lcd_flush_area() writes RGB565 pixels to the LCD window through SPI.

Memory cuts:
  LV_MEM_SIZE is 2 KB.
  LVGL draw buffer is 240 x 4 pixels.
  Default font is lv_font_unscii_8.
  Only LV_USE_LABEL is enabled at widget level.
  LV_USE_BAR, demos, examples, filesystem backends, image decoders, and themes
  are disabled.

Build result:
  Compiler: ARMClang V6.22
  Program Size: Code=52666 RO-data=2274 RW-data=40 ZI-data=10080
  Build status: 0 Error(s), 0 Warning(s)

Notes:
  This project is stored under project\04_FreeRTOS_LVGL_Display and does not
  modify the official GD32 demo suite.
