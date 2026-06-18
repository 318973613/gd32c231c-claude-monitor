GD32C231C-EVAL FreeRTOS status link

This is the third evaluation stage project. It ports the custom USART status
link to FreeRTOS instead of using the previous polling/main-loop structure.

Target board:
  GD32C231C-EVAL

Target device:
  GD32C231C8, Cortex-M23

FreeRTOS source:
  Kernel files are copied from:
    https://github.com/FreeRTOS/FreeRTOS-Kernel
  ARMClang builds the GCC Cortex-M23 port according to the FreeRTOS note:
    portable\ARMClang\Use-the-GCC-ports.txt

Selected FreeRTOS port:
  portable\GCC\ARM_CM23_NTZ\non_secure
  portable\MemMang\heap_4.c

RTOS tasks:
  LED task     -> heartbeat and four LED status indicators
  SERIAL task  -> parse command lines from the USART RX queue
  MONITOR task -> print tick/queue/free-heap status every 5 seconds

USART:
  USART0 TX: PA9
  USART0 RX: PA10
  Baudrate : 115200
  Format   : 8 data bits, no parity, 1 stop bit

RX design:
  USART0 RBNE interrupt receives one byte.
  The ISR pushes bytes into a FreeRTOS queue with xQueueSendFromISR().
  The SERIAL task receives bytes, builds one command line, and handles it.

Commands:
  PING or P  -> board replies PONG
  IDLE or I  -> clear running/error state
  RUN  or R  -> set running state
  DONE or D  -> clear state and flash all LEDs once
  ERR  or E  -> set error state
  HELP or H  -> print command list

LED status:
  LED1: heartbeat
  LED2: RUN state
  LED3: RX pulse
  LED4: ERROR state

Keil project:
  MDK-ARM\GD32C231C_FreeRTOS_Link.uvprojx

Dependency:
  ..\GD32C2x1_Firmware_Library
