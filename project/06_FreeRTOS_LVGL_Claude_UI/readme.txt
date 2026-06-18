GD32C231C-EVAL FreeRTOS LVGL Claude UI

This is the sixth evaluation stage project. It is based on
05_FreeRTOS_LVGL_DHT11, keeps FreeRTOS + LVGL + DHT11, and adds a Claude-style
dashboard, a software clock configured by USART0, and a four-LED flow effect.

Target board:
  GD32C231C-EVAL

Target device:
  GD32C231C8, Cortex-M23
  Flash: 64 KB
  SRAM : 12 KB

Keil project:
  MDK-ARM\GD32C231C_FreeRTOS_Claude_UI.uvprojx

Output:
  MDK-ARM\Objects\FreeRTOS_Claude_UI.hex

Module layout:
  main.c
    Owns FreeRTOS task creation and DHT11 sampling.

  app_dashboard.c / app_dashboard.h
    Owns LCD, LVGL port, screen creation and label refresh.

  app_serial_clock.c / app_serial_clock.h
    Owns USART0 PA9/PA10, interrupt receive, command parsing and software clock.

  app_led_effect.c / app_led_effect.h
    Owns all four board LEDs and non-blocking visual effects.

  app_status.h
    Shared lightweight state structures.

Pin plan:
  DHT11 DATA -> PB0
  USART0 TX  -> PA9
  USART0 RX  -> PA10
  LED1       -> PA15
  LED2       -> PD1
  LED3       -> PD3
  LED4       -> PB4

Serial clock command:
  115200 8N1

  TIME=HH:MM:SS
  T=HH:MM:SS

  Examples:
    TIME=12:34:56
    T=08:00:00

Runtime behavior:
  The screen uses a black and warm-orange pixel style inspired by the provided
  Claude reference image. No bitmap image is stored in flash; the mascot is
  drawn as a small LVGL text pixel pattern to save memory.

  The clock shows --:--:-- until USART0 receives a valid time command. After
  that, the time advances from the FreeRTOS tick.

  DHT11 keeps the PB0 acquisition from stage 05. The GUI displays temperature,
  humidity, DHT11 status, sample count, error count and USART receive count.

  All four LEDs are used:
    Clock not set: cross blink.
    DHT11 error  : warning blink.
    Normal run   : flowing LED pattern.
    USART receive: short sparkle flash.

Build result:
  Compiler: ARMClang V6.22
  Program Size: Code=58554 RO-data=2438 RW-data=56 ZI-data=11200
  Build status: 0 Error(s), 0 Warning(s)

Notes:
  This project is stored under project\06_FreeRTOS_LVGL_Claude_UI and does not
  modify the official GD32 demo suite or the previous working stages.
