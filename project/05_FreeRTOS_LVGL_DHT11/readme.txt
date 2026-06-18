GD32C231C-EVAL FreeRTOS LVGL DHT11

This is the fifth evaluation stage project. It is based on
04_FreeRTOS_LVGL_Display, keeps FreeRTOS + LVGL running, and adds a DHT11
temperature/humidity acquisition task.

Target board:
  GD32C231C-EVAL

Target device:
  GD32C231C8, Cortex-M23
  Flash: 64 KB
  SRAM : 12 KB

Keil project:
  MDK-ARM\GD32C231C_FreeRTOS_DHT11.uvprojx

Output:
  MDK-ARM\Objects\FreeRTOS_DHT11.hex

DHT11 wiring:
  VCC  -> 3V3
  GND  -> GND
  DATA -> PB0

Pull-up:
  DATA needs a pull-up to 3V3. Most DHT11 modules already include it. If using
  a bare DHT11 sensor, add a 4.7 kOhm to 10 kOhm pull-up resistor from DATA to
  3V3.

Pin plan:
  PB0 is selected for DHT11 data because the LCD already uses PA8, PC6, PC7,
  PB13, PB14 and PB15. USART0 PA9/PA10 is reserved for the later Codex status
  protocol stage.

Runtime tasks:
  GUI task:
    Initializes LCD and LVGL.
    Calls lv_timer_handler().
    Updates the temperature, humidity and DHT11 status labels every 500 ms.

  DHT11 task:
    Initializes PB0.
    Reads DHT11 every 2 seconds.
    Stores the latest sample status for the GUI task.

DHT11 timing design:
  The start low pulse uses vTaskDelay().
  The short response/data timing window is sampled with microsecond busy-wait
  loops inside a short FreeRTOS critical section. This prevents SysTick or other
  interrupts from stretching the 26 us / 70 us DHT11 high pulse measurement.

LCD/LVGL:
  Resolution: 240 x 320 portrait
  LCD direction is rotated 180 degrees by MADCTL.
  LVGL keeps the small-resource configuration from the previous stage.

Build result:
  Compiler: ARMClang V6.22
  Program Size: Code=56026 RO-data=2330 RW-data=56 ZI-data=11120
  Build status: 0 Error(s), 0 Warning(s)

Notes:
  This project is stored under project\05_FreeRTOS_LVGL_DHT11 and does not
  modify the official GD32 demo suite or the previous LVGL-only project.
