# GD32C231C-EVAL 测评工程目录

这个目录用于放测评文章中实际使用的工程文件。每个阶段先在这里准备可打开、可编译、可下载的工程，再回到 `D:\gd32c231c\doc\测评合集.md` 继续写教程。

## 当前阶段

### 01_Custom_LED_Animation

工程路径：

```text
D:\gd32c231c\project\01_Custom_LED_Animation
```

Keil 工程：

```text
D:\gd32c231c\project\01_Custom_LED_Animation\MDK-ARM\GD32C231C_Custom_LED.uvprojx
```

这个工程用于验证最基础的下载流程和 GPIO 输出能力。它不是直接使用官方 `01_GPIO_Running_LED` 例程，而是单独新建的自定义工程，效果也重新写成了 4 LED 组合动画：

- 上电后 4 个 LED 全亮/全灭闪烁 3 次；
- LED 从左到右累积点亮，再反向收尾；
- LED1+LED4 和 LED2+LED3 做左右对称扫描；
- 最后用 4 个 LED 显示 0 到 15 的二进制计数。

这个效果会用到 LED1、LED2、LED3、LED4 全部四个灯，便于后续拍摄运行结果。

### 02_USART_Status_Link

工程路径：

```text
D:\gd32c231c\project\02_USART_Status_Link
```

Keil 工程：

```text
D:\gd32c231c\project\02_USART_Status_Link\MDK-ARM\GD32C231C_USART_Link.uvprojx
```

这个工程用于验证 USART0 串口通信。它不是直接使用官方 `04_USART_Printf` 例程，而是单独写了一个状态命令链路：

- 串口参数：USART0，PA9/PA10，115200 8N1；
- PC 发送 `PING`，开发板回复 `PONG`；
- PC 发送 `RUN`、`IDLE`、`DONE`、`ERR`，开发板回 ACK；
- LED1 做心跳，LED2 表示运行状态，LED3 表示收到串口数据，LED4 表示错误状态。

这个阶段是后面 Codex 状态监视器的通信基础。

## 依赖目录

Keil 工程使用官方相对路径引用标准外设库，所以这里保留了同级的固件库目录：

```text
D:\gd32c231c\project\GD32C2x1_Firmware_Library
```

不要单独移动 `MDK-ARM` 目录，否则工程里的 `..\..\GD32C2x1_Firmware_Library` 路径会失效。
