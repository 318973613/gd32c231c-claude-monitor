# Claude Board Monitor Desktop

Windows desktop monitor for Claude Code + GD32C231C boards.

The app is intentionally a single executable:

- Double-click `ClaudeBoardMonitor.exe` to open the monitor UI.
- Claude Code calls the same executable with `--hook` to forward hook events.
- No Python or Node.js runtime is required for end users.

## Published build

Current portable build:

```text
D:\gd32c231c\tools\claude_monitor_desktop\publish\win-x64\ClaudeBoardMonitor.exe
```

This is a self-contained Windows x64 build. It includes the .NET runtime and can be copied to another Windows x64 PC.

## User steps

1. Run `ClaudeBoardMonitor.exe`.
2. Select the board COM port and click `Connect`.
3. Click `Install Claude Hooks`.
4. Restart Claude Code.
5. Keep `ClaudeBoardMonitor.exe` running while using Claude Code.

When Claude requests approval, either:

- Press `PA0` on the board to approve.
- Press `PA4` on the board to deny.
- Or use the GUI buttons.

## Board serial protocol

PC to board:

```text
AI=IDLE
AI=WORKING
AI=TOOL:<tool>
AI=WAIT_APPROVE:<tool>
AI=APPROVED
AI=DENIED
AI=DONE
AI=ERROR:<reason>
```

Board to PC:

```text
BTN=APPROVE
BTN=DENY
```

## Build

```powershell
dotnet publish D:\gd32c231c\tools\claude_monitor_desktop\ClaudeBoardMonitor.csproj `
  -c Release `
  -r win-x64 `
  --self-contained true `
  -p:PublishSingleFile=true `
  -p:IncludeNativeLibrariesForSelfExtract=true `
  -p:EnableCompressionInSingleFile=true `
  -o D:\gd32c231c\tools\claude_monitor_desktop\publish\win-x64
```

## Hook install behavior

The app backs up the user's Claude settings before modifying:

```text
%USERPROFILE%\.claude\settings.json
```

It only inserts or updates hook entries containing `ClaudeBoardMonitor` or legacy `claude_hook.py`; it does not overwrite `apiKey`, `model`, `theme`, or unrelated hooks.
