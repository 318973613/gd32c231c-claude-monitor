# Claude 开发板监控台

这是一个 Windows x64 上位机，用于连接 Claude Code、GD32C231C 开发板和串口屏幕状态。

发布包里有两个程序：

- `ClaudeBoardMonitor.exe`：双击运行的中文图形界面。
- `ClaudeBoardHook.exe`：Claude Code Hooks 调用的控制台助手，不需要手动打开。

最终用户不需要安装 .NET、Python 或 Node.js。

## 使用步骤

1. 双击运行 `ClaudeBoardMonitor.exe`。
2. 选择开发板 COM 口，点击“连接”。
3. 软件会自动向开发板发送当前电脑时间：`TIME=HH:MM:SS`。
4. 点击“安装 / 更新 Hooks”，会同时写入 Hooks 和 statusLine。
5. 重启 Claude Code。
6. 使用 Claude Code 时保持上位机运行。

默认会启用“外部审批模式”：Claude 权限请求会等待上位机、PA0 或 PA4 决定：

- PA0：同意。
- PA4：拒绝。
- GUI 的“同意 / 拒绝”按钮也可以审批。

如果关闭“外部审批模式”，Claude 会使用自己的终端原生确认提示。此时上位机和开发板会明显提示“请看终端”，但 PA0、PA4 和 GUI 按钮不能控制这一次提示，必须在 Claude 终端里按 1/2/3。

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

## 可分发文件

便携发布目录：

```text
D:\gd32c231c\tools\claude_monitor_desktop\publish\win-x64
```

压缩包：

```text
D:\gd32c231c\tools\claude_monitor_desktop\ClaudeBoardMonitor-win-x64.zip
```

分享给别人时发这个 zip，不要发 `bin` 或 `obj` 里的中间产物。

## Hook 安装行为

软件会备份并修改：

```text
%USERPROFILE%\.claude\settings.json
```

它只插入或更新与 `ClaudeBoardHook`、`ClaudeBoardMonitor` 或旧版 `claude_hook.py` 相关的 Hook 项，不会覆盖 `apiKey`、`model`、`theme` 或其它无关配置。

## 状态联动

- Hooks 驱动 Claude 事件：空闲、工作中、工具调用、终端原生确认、等待外部审批、同意、拒绝、错误。
- statusLine 驱动运行状态：模型名、上下文使用率、剩余率、限额使用率、当前会话费用。
- 开发板 LCD 会显示模型和百分比；如果 Claude 当前版本没有提供某项字段，会显示为未知。
- LED 根据 Claude 状态变化：
  - 空闲：慢心跳。
  - 工作中 / 工具调用：流水灯。
  - Claude 原生终端确认：对角交替闪烁，需要去终端按 1/2/3。
  - 等待外部审批：左右交替闪烁，可按 PA0 / PA4 或 GUI 按钮。
  - 同意：全亮脉冲。
  - 拒绝：单灯警示。
  - 错误：全灯闪烁。
