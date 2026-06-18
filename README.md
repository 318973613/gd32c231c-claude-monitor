# GD32C231C Claude Monitor

基于兆易创新 GD32C231C-EVAL 开发板的 Claude 状态监视器，集成 FreeRTOS 多任务调度、LVGL 图形界面、DHT11 温湿度采集和外部 Flash UI 资源方案。

## 功能特性

- **FreeRTOS 多任务调度**：LED、串口、传感器、显示独立任务
- **LVGL 图形界面**：240x320 SPI LCD 显示
- **DHT11 温湿度采集**：实时显示环境数据
- **外部 Flash UI**：GD25Q16 存储背景图，节省片内资源
- **Claude 状态显示**：空闲/工作状态小人动画
- **串口时钟设置**：`TIME=12:34:56` 或 `T=08:00:00`

## 硬件平台

| 项目 | 规格 |
|------|------|
| 芯片 | GD32C231C8 (Cortex-M23) |
| 主频 | 48 MHz |
| Flash | 64 KB |
| SRAM | 12 KB |
| LCD | 240x320 SPI (ILI9341 兼容) |
| 传感器 | DHT11 (PB0) |
| 外部 Flash | GD25Q16 (SPI1) |

## 工程结构

```
project/
├── 01_Custom_LED_Animation/      # GPIO 流水灯验证
├── 02_USART_Status_Link/         # 串口状态链路
├── 03_FreeRTOS_Status_Link/      # FreeRTOS 移植
├── 04_FreeRTOS_LVGL_Display/     # LVGL 屏幕显示
├── 05_FreeRTOS_LVGL_DHT11/       # DHT11 传感器接入
├── 07_SPI_FLASH_AssetWriter/     # 外部 Flash 资源写入
├── 08_FreeRTOS_LVGL_Claude_Flash_UI/  # Claude 状态监视器
└── GD32C2x1_Firmware_Library/    # 标准外设库
```

## 快速开始

### 1. 下载资源到外部 Flash

```
下载 project\07_SPI_FLASH_AssetWriter\MDK-ARM\Objects\SPI_FLASH_AssetWriter.hex
等待串口输出: OK: assets stored at external flash 0x000000
```

### 2. 下载应用固件

```
下载 project\08_FreeRTOS_LVGL_Claude_Flash_UI\MDK-ARM\Objects\FreeRTOS_Claude_Flash_UI.hex
```

### 3. 串口设置时钟

```
TIME=23:05:00
```

## 串口命令

| 命令 | 响应 | 说明 |
|------|------|------|
| `TIME=12:34:56` | `OK TIME=12:34:56` | 设置时钟 |
| `T=08:00:00` | `OK TIME=08:00:00` | 设置时钟（简写） |
| `PING` / `P` | `PONG` | 链路测试 |
| `RUN` / `R` | `ACK STATE=RUNNING` | 设置运行状态 |
| `IDLE` / `I` | `ACK STATE=IDLE` | 设置空闲状态 |
| `ERR` / `E` | `ACK STATE=ERROR` | 设置错误状态 |
| `DONE` / `D` | `ACK STATE=DONE` | 设置完成状态 |

## 引脚分配

| 功能 | 引脚 |
|------|------|
| LED1-4 | PA15, PD1, PD3, PB4 |
| USART0 TX/RX | PA9, PA10 |
| SPI1 SCK/MISO/MOSI | PB13, PB14, PB15 |
| LCD CS/DC/RST | PA8, PC7, PC6 |
| Flash CS | PB11 |
| DHT11 DATA | PB0 |

## 开发环境

- Keil MDK 5.27+
- GD32C2x1 Pack V1.1.0
- FreeRTOS Kernel
- LVGL v8.4.0

## 许可证

MIT License
