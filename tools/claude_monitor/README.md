# Claude board monitor

This tool links Claude Code hook events to the GD32C231C board through USART0.

## Flow

1. Claude Code fires hooks such as `UserPromptSubmit`, `PreToolUse`, `PostToolUse`, `Notification`, and `Stop`.
2. `claude_hook.py` receives the hook JSON from stdin.
3. The hook script posts events to the local monitor at `127.0.0.1:8765`.
4. `claude_monitor.py` displays the state and sends line commands to the board over USART0.
5. When a risky tool needs approval, the monitor waits for either:
   - GUI `Approve` / `Deny`
   - Board `PA0` / `PA4`

## Run the monitor

```powershell
cd D:\gd32c231c\tools\claude_monitor
"C:\Users\小周\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe" .\claude_monitor.py
```

Set the COM port, then click `Connect`.

Serial settings:

- `115200`
- `8N1`
- USART0 board pins: `PA9 TX`, `PA10 RX`

## Claude Code hook setup

Use `claude_settings_snippet.json` as a reference and merge the `hooks` object into:

```text
C:\Users\小周\.claude\settings.json
```

Do not overwrite the whole file because it may contain your `apiKey`.

Restart Claude Code after editing settings. Hooks are loaded when Claude Code starts.

## Serial protocol

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

Current button mapping:

- `PA0`: approve
- `PA4`: deny

The board still accepts the existing clock commands:

```text
TIME=12:34:56
T=08:00:00
```

## Claude status source

The monitor uses Claude Code hooks instead of terminal scraping. Hooks provide structured JSON with fields such as:

- `hook_event_name`
- `session_id`
- `cwd`
- `tool_name`
- `tool_input`
- `tool_result`
- `user_prompt`

`PreToolUse` is used to approve risky tools before they run. Newer Claude Code builds can also emit `PermissionRequest` when a permission dialog appears; this monitor handles both paths after it receives a GUI or board decision.
