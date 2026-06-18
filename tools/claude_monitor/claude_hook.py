"""Claude Code hook bridge for the GD32C231C monitor."""

from __future__ import annotations

import json
import sys
import urllib.error
import urllib.request
from typing import Any


MONITOR = "http://127.0.0.1:8765"
APPROVAL_TOOLS = {
    "Bash",
    "Write",
    "Edit",
    "MultiEdit",
    "NotebookEdit",
}


def post_json(path: str, payload: dict[str, Any], timeout: int) -> dict[str, Any]:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        MONITOR + path,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    with opener.open(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8") or "{}")


def emit(data: dict[str, Any]) -> None:
    sys.stdout.write(json.dumps(data, ensure_ascii=False))
    sys.stdout.flush()


def handle_pre_tool(payload: dict[str, Any]) -> None:
    tool = str(payload.get("tool_name", ""))
    try:
        if tool in APPROVAL_TOOLS:
            result = post_json("/approval", payload, timeout=115)
            decision = "allow" if result.get("decision") == "allow" else "deny"
            emit(
                {
                    "hookSpecificOutput": {
                        "hookEventName": "PreToolUse",
                        "permissionDecision": decision,
                        "permissionDecisionReason": result.get("reason", "board monitor decision"),
                    },
                    "systemMessage": f"Board monitor decision: {decision}",
                }
            )
        else:
            post_json("/event", payload, timeout=3)
            emit({"continue": True, "suppressOutput": True})
    except Exception as exc:
        emit(
            {
                "hookSpecificOutput": {
                    "hookEventName": "PreToolUse",
                    "permissionDecision": "ask",
                    "permissionDecisionReason": f"board monitor unavailable: {exc}",
                },
                "systemMessage": "Board monitor unavailable; falling back to Claude permission prompt.",
            }
        )


def handle_permission_request(payload: dict[str, Any]) -> None:
    try:
        result = post_json("/approval", payload, timeout=115)
        behavior = "allow" if result.get("decision") == "allow" else "deny"
        emit(
            {
                "hookSpecificOutput": {
                    "hookEventName": "PermissionRequest",
                    "decision": {
                        "behavior": behavior,
                        "message": result.get("reason", "board monitor decision"),
                    },
                },
                "systemMessage": f"Board monitor permission decision: {behavior}",
            }
        )
    except Exception as exc:
        emit(
            {
                "continue": True,
                "suppressOutput": True,
                "systemMessage": f"Board monitor unavailable for PermissionRequest: {exc}",
            }
        )


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read() or "{}")
    except json.JSONDecodeError as exc:
        emit({"continue": True, "systemMessage": f"Hook JSON parse failed: {exc}"})
        return 0

    event = str(payload.get("hook_event_name", ""))
    if event == "PreToolUse":
        handle_pre_tool(payload)
        return 0
    if event == "PermissionRequest":
        handle_permission_request(payload)
        return 0

    try:
        post_json("/event", payload, timeout=3)
    except (OSError, urllib.error.URLError) as exc:
        emit({"continue": True, "suppressOutput": True, "systemMessage": f"Board monitor offline: {exc}"})
        return 0

    if event == "Stop":
        emit({"decision": "approve", "reason": "monitor event recorded", "suppressOutput": True})
    else:
        emit({"continue": True, "suppressOutput": True})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
