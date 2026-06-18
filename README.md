# GD32C231C Claude Monitor

这是一个基于 GD32C231C-EVAL 开发板的 Claude Code 状态监控项目。项目把 PC 上位机、Claude Code Hooks、串口协议和 GD32C231C 小屏状态显示串起来，用开发板 LCD、LED、PA0/PA4 按键显示和处理 Claude 的工作状态与审批请求。

## 当前效果

- PC 上位机通过串口连接开发板，启动后自动下发电脑时间。
- 上位机可安装 / 更新 Claude Code Hooks 和 statusLine 配置。
- Claude 工作中、工具调用、等待审批、同意、拒绝、错误等状态会同步到开发板。
- PA0 表示同意，PA4 表示拒绝；上位机按钮也可以处理同一类审批。
- 开发板 LCD 显示空闲 / 工作动画、模型名、上下文使用率、剩余额度和费用。
- LED 根据 Claude 状态切换不同灯效，等待审批时会有明显提示。

## 硬件

| 模块 | 连接 |
| --- | --- |
| MCU | GD32C231C8, Cortex-M23, 64 KB Flash, 12 KB SRAM |
| LCD | SPI LCD, 240x320, SPI1 |
| 外部 Flash | GD25Q16, JEDEC ID `0xC84015`, SPI1 |
| Flash 引脚 | PB13 SCK, PB14 MISO, PB15 MOSI, PB11 CS |
| LCD 引脚 | PB13 SCK, PB14 MISO, PB15 MOSI, PA8 CS, PC7 DC, PC6 RST |
| USART0 | PA9 TX, PA10 RX, 115200 8N1 |
| DHT11 | PB0 |
| 用户按键 | PA0 同意, PA4 拒绝 |
| LED | PA15, PD1, PD3, PB4 |

LCD 和 GD25Q16 共用 SPI1，但 SPI 模式不同。固件在读取外部 Flash 前调用 `spi_flash_init()`，绘制 LCD 前调用 `lcd_bus_init()` 切换总线配置。

## 仓库结构

```text
doc/
  测评合集.md                         # 项目文章，保留了截图占位

project/
  01_Custom_LED_Animation/            # GPIO/LED 基础验证
  02_USART_Status_Link/               # USART0 状态链路
  03_FreeRTOS_Status_Link/            # FreeRTOS 移植
  04_FreeRTOS_LVGL_Display/           # LCD/LVGL 验证
  05_FreeRTOS_LVGL_DHT11/             # DHT11 + LCD
  06_FreeRTOS_LVGL_Claude_UI/         # 早期 LVGL UI 方案
  07_SPI_FLASH_AssetWriter/           # GD25Q16 资源写入工程
  08_FreeRTOS_LVGL_Claude_Flash_UI/   # 当前主应用工程
  GD32C2x1_Firmware_Library/          # Keil 工程依赖的 GD32 标准外设库

tools/
  claude_monitor_desktop/             # Windows 中文上位机源码
  claude_monitor_hook/                # Claude Hook 控制台程序源码
  make_claude_assets.py               # UI 资源生成辅助脚本

release/
  ClaudeBoardMonitor-win-x64.zip      # 给最终用户使用的上位机压缩包
  firmware/                           # 可直接烧录的 hex
```

## 快速使用

1. 烧录外部 Flash 资源：

```text
release\firmware\SPI_FLASH_AssetWriter.hex
```

串口看到以下信息表示资源已经写入 GD25Q16：

```text
OK: assets stored at external flash 0x000000
```

2. 烧录主应用：

```text
release\firmware\FreeRTOS_Claude_Flash_UI.hex
```

3. 解压并运行上位机：

```text
release\ClaudeBoardMonitor-win-x64.zip
```

双击 `ClaudeBoardMonitor.exe`，选择开发板 COM 口并连接。连接成功后，上位机会自动发送 `TIME=HH:MM:SS` 同步电脑时间。

4. 点击上位机里的“安装 / 更新 Hooks”，然后重启 Claude Code。

保持上位机运行，Claude Code 的状态和审批事件就会通过串口同步到开发板。

## 串口协议

PC 发给开发板：

```text
AI=IDLE
AI=WORKING
AI=TOOL:<tool>
AI=NATIVE_APPROVE:<tool>
AI=WAIT_APPROVE:<tool>
AI=APPROVED
AI=DENIED
AI=DONE
AI=ERROR:<reason>
TIME=HH:MM:SS
MODEL=<model>
CTX=<used_percent>,<remaining_percent>
RATE=<used_percent>,<remaining_percent>
COST=<usd_cents>
```

开发板发给 PC：

```text
BTN=APPROVE
BTN=DENY
```

## 开发说明

- Keil MDK 工程在各阶段目录的 `MDK-ARM` 下。
- 当前主应用是 `project\08_FreeRTOS_LVGL_Claude_Flash_UI`。
- UI 大图资源不放片内 SRAM，而是打包成 `ui_assets.bin` 后写入 GD25Q16。
- 08 工程保留 LVGL 目录和历史移植环境，但最终动态覆盖层主要走裸 LCD 绘制，以减少 12 KB SRAM 压力。
- 上位机使用 C# WinForms，发布脚本在 `tools\claude_monitor_desktop\publish_win_x64.ps1`。

## 文档

完整过程文章见：

```text
doc\测评合集.md
```

文章末尾保留了需要补图的位置，后续可以按实际运行界面继续补截图。

## 许可证说明

本仓库包含 GD32 标准外设库、FreeRTOS、LVGL 等第三方代码或派生工程，相关部分遵循各自原始许可证。文章和示例工程仅用于学习、测评和复现项目过程。
