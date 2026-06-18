GD32C231C-EVAL USART0 status link

This is the second evaluation stage project. It is a custom USART0 command
link, not a direct copy of the official USART printf demo.

Target board:
  GD32C231C-EVAL

Target device:
  GD32C231C8

USART:
  USART0 TX: PA9
  USART0 RX: PA10
  Baudrate : 115200
  Format   : 8 data bits, no parity, 1 stop bit

RX mode:
  USART0 RBNE interrupt receives bytes.
  The main loop only handles completed command lines and LED state.

Commands:
  PING or P  -> board replies PONG
  IDLE or I  -> clear running/error state
  RUN  or R  -> set running state
  DONE or D  -> clear state and flash all LEDs once
  ERR  or E  -> set error state
  HELP or H  -> print command list

Debug output:
  Every command prints RX length, ASCII text, and HEX bytes.
  PING should appear as:
    RX[4]: PING
    HEX: 50 49 4E 47

LED status:
  LED1: heartbeat
  LED2: RUN state
  LED3: RX pulse
  LED4: ERROR state

Keil project:
  MDK-ARM\GD32C231C_USART_Link.uvprojx

Dependency:
  ..\GD32C2x1_Firmware_Library
