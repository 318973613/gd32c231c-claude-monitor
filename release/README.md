# Release Files

这个目录放可以直接使用的发布产物。

## Windows 上位机

```text
ClaudeBoardMonitor-win-x64.zip
```

解压后双击 `ClaudeBoardMonitor.exe`。发布包内已经包含 `ClaudeBoardHook.exe`，普通用户不需要单独运行 Hook 程序。

## 固件烧录顺序

先烧录外部 Flash 资源写入工程：

```text
firmware\SPI_FLASH_AssetWriter.hex
```

串口确认看到：

```text
OK: assets stored at external flash 0x000000
```

再烧录主应用：

```text
firmware\FreeRTOS_Claude_Flash_UI.hex
```

主应用启动后，串口应看到：

```text
[08] dashboard init
[08] flash asset ok
[08] background draw done
[08] gui loop running
```

## 硬件连接

USART0 使用 `PA9 TX`、`PA10 RX`，串口参数为 `115200 8N1`。开发板上 `PA0` 是同意审批，`PA4` 是拒绝审批。
