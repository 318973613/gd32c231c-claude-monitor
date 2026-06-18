GD32C231C-EVAL custom four-led animation

This project is used for the first evaluation stage. It is based on the
GD32C231C-EVAL board resources, but the LED behavior is customized instead of
using the original official running LED demo directly.

Target board:
  GD32C231C-EVAL

Target device:
  GD32C231C8

LED mapping:
  LED1: PA15
  LED2: PD1
  LED3: PD3
  LED4: PB4

Animation sequence:
  1. Blink all four LEDs three times after reset.
  2. Fill LEDs from LED1 to LED4, then clear from LED1 to LED4.
  3. Mirror scan with LED1+LED4 and LED2+LED3.
  4. Show a 4-bit counter from 0 to 15 on LED1..LED4.

Keil project:
  MDK-ARM\GD32C231C_Custom_LED.uvprojx

Dependency:
  ..\GD32C2x1_Firmware_Library
