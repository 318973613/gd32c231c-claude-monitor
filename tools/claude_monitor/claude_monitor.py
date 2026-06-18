"""Claude/Codex state monitor for GD32C231C USART link."""

from __future__ import annotations

from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import queue
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox
from typing import Any, Optional

from serial_win32 import Win32Serial


HTTP_HOST = "127.0.0.1"
HTTP_PORT = 8765
BAUDRATE = 115200
APP_TITLE = "Claude Board Monitor"


@dataclass
class PendingApproval:
    request_id: int
    tool_name: str
    summary: str
    created_at: float
    event: threading.Event
    decision: Optional[str] = None


class MonitorState:
    def __init__(self) -> None:
        self.gui_queue: "queue.Queue[tuple[str, Any]]" = queue.Queue()
        self.serial = Win32Serial()
        self.lock = threading.Lock()
        self.pending: Optional[PendingApproval] = None
        self.request_seq = 0
        self.last_status = "IDLE"

    def post_gui(self, kind: str, payload: Any = None) -> None:
        self.gui_queue.put((kind, payload))

    def send_board(self, line: str) -> None:
        try:
            if self.serial.connected:
                self.serial.write_line(line)
                self.post_gui("log", f"PC -> Board  {line}")
            else:
                self.post_gui("log", f"Board offline, skipped: {line}")
        except Exception as exc:
            self.post_gui("log", f"Serial write failed: {exc}")

    def set_status(self, status: str, board_line: Optional[str] = None) -> None:
        self.last_status = status
        self.post_gui("status", status)
        if board_line:
            self.send_board(board_line)

    def open_approval(self, tool_name: str, summary: str) -> PendingApproval:
        with self.lock:
            self.request_seq += 1
            pending = PendingApproval(
                request_id=self.request_seq,
                tool_name=tool_name,
                summary=summary[:80],
                created_at=time.time(),
                event=threading.Event(),
            )
            self.pending = pending
        self.post_gui("approval", pending)
        self.set_status("WAIT_APPROVE", f"AI=WAIT_APPROVE:{tool_name[:20]}")
        return pending

    def decide(self, decision: str, source: str) -> bool:
        with self.lock:
            pending = self.pending
            if pending is None:
                self.post_gui("log", f"{source}: no pending approval")
                return False
            pending.decision = decision
            pending.event.set()
            self.pending = None
        if decision == "allow":
            self.set_status("APPROVED", "AI=APPROVED")
        else:
            self.set_status("DENIED", "AI=DENIED")
        self.post_gui("approval_done", {"decision": decision, "source": source})
        return True


STATE = MonitorState()


def summarize_hook(payload: dict[str, Any]) -> tuple[str, str]:
    event = payload.get("hook_event_name", "Unknown")
    tool = payload.get("tool_name", "")
    if event in ("PreToolUse", "PermissionRequest", "PostToolUse", "PostToolUseFailure"):
        tool_input = payload.get("tool_input", {})
        if isinstance(tool_input, dict):
            hint = tool_input.get("file_path") or tool_input.get("command") or tool_input.get("path")
        else:
            hint = ""
        summary = f"{event} {tool} {hint}".strip()
        return event, summary
    if event == "UserPromptSubmit":
        prompt = str(payload.get("user_prompt", ""))[:80]
        return event, f"User prompt: {prompt}"
    if event == "Notification":
        return event, str(payload.get("message", "Notification"))[:80]
    if event == "Stop":
        return event, f"Stop: {payload.get('reason', '')}"
    return event, event


class HookHandler(BaseHTTPRequestHandler):
    server_version = "ClaudeBoardMonitor/0.1"

    def do_POST(self) -> None:  # noqa: N802
        try:
            length = int(self.headers.get("Content-Length", "0"))
            payload = json.loads(self.rfile.read(length).decode("utf-8") or "{}")
            if self.path == "/event":
                self._handle_event(payload)
            elif self.path == "/approval":
                self._handle_approval(payload)
            else:
                self._send_json({"ok": False, "error": "not found"}, 404)
        except Exception as exc:
            STATE.post_gui("log", f"HTTP error: {exc}")
            self._send_json({"ok": False, "error": str(exc)}, 500)

    def log_message(self, fmt: str, *args: Any) -> None:
        return

    def _handle_event(self, payload: dict[str, Any]) -> None:
        event, summary = summarize_hook(payload)
        STATE.post_gui("log", f"Hook {summary}")
        if event == "UserPromptSubmit":
            STATE.set_status("WORKING", "AI=WORKING")
        elif event == "PreToolUse":
            tool = str(payload.get("tool_name", "TOOL"))
            STATE.set_status(f"TOOL {tool}", f"AI=TOOL:{tool[:24]}")
        elif event.startswith("PostTool"):
            STATE.set_status("WORKING", "AI=WORKING")
        elif event == "Notification":
            STATE.set_status("NOTICE", "AI=WORKING")
        elif event == "Stop":
            STATE.set_status("DONE", "AI=DONE")
        self._send_json({"ok": True})

    def _handle_approval(self, payload: dict[str, Any]) -> None:
        tool = str(payload.get("tool_name", "TOOL"))
        _, summary = summarize_hook(payload)
        pending = STATE.open_approval(tool, summary)
        ok = pending.event.wait(timeout=110)
        if not ok:
            with STATE.lock:
                if STATE.pending is pending:
                    STATE.pending = None
            STATE.set_status("TIMEOUT", "AI=ERROR:APPROVAL_TIMEOUT")
            self._send_json({"decision": "deny", "reason": "approval timeout"})
            return
        self._send_json({"decision": pending.decision or "deny", "reason": "board/user decision"})

    def _send_json(self, data: dict[str, Any], status: int = 200) -> None:
        body = json.dumps(data).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def start_http_server() -> ThreadingHTTPServer:
    server = ThreadingHTTPServer((HTTP_HOST, HTTP_PORT), HookHandler)
    thread = threading.Thread(target=server.serve_forever, name="hook-http", daemon=True)
    thread.start()
    return server


class MonitorApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("820x560")
        self.minsize(760, 500)
        self.configure(bg="#15120f")
        self.http_server = start_http_server()
        self._build_ui()
        self._tick()
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        STATE.post_gui("log", f"HTTP hook server: http://{HTTP_HOST}:{HTTP_PORT}")

    def _build_ui(self) -> None:
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TFrame", background="#15120f")
        style.configure("TLabel", background="#15120f", foreground="#efe5d8")
        style.configure("TButton", padding=8, font=("Segoe UI", 10))
        style.configure("Accent.TButton", padding=10, font=("Segoe UI", 11, "bold"))

        root = ttk.Frame(self, padding=18)
        root.pack(fill="both", expand=True)

        header = ttk.Frame(root)
        header.pack(fill="x")
        ttk.Label(header, text="Claude Board Monitor", font=("Segoe UI", 22, "bold")).pack(side="left")
        self.status_label = ttk.Label(header, text="IDLE", font=("Consolas", 18, "bold"))
        self.status_label.pack(side="right")

        conn = ttk.Frame(root)
        conn.pack(fill="x", pady=(18, 10))
        ttk.Label(conn, text="COM Port").pack(side="left")
        self.port_var = tk.StringVar(value="COM3")
        ttk.Entry(conn, textvariable=self.port_var, width=12).pack(side="left", padx=8)
        ttk.Button(conn, text="Connect", command=self._connect).pack(side="left", padx=4)
        ttk.Button(conn, text="Disconnect", command=self._disconnect).pack(side="left", padx=4)
        ttk.Button(conn, text="AI Idle", command=lambda: STATE.set_status("IDLE", "AI=IDLE")).pack(side="left", padx=10)
        ttk.Button(conn, text="AI Working", command=lambda: STATE.set_status("WORKING", "AI=WORKING")).pack(side="left")

        approval = ttk.Frame(root)
        approval.pack(fill="x", pady=12)
        self.pending_label = ttk.Label(approval, text="No pending approval", font=("Segoe UI", 12))
        self.pending_label.pack(side="left", fill="x", expand=True)
        ttk.Button(approval, text="Approve", style="Accent.TButton", command=lambda: STATE.decide("allow", "GUI")).pack(side="right", padx=4)
        ttk.Button(approval, text="Deny", style="Accent.TButton", command=lambda: STATE.decide("deny", "GUI")).pack(side="right", padx=4)

        self.log = tk.Text(root, height=20, bg="#201b17", fg="#f3eadf", insertbackground="#f3eadf",
                           relief="flat", font=("Consolas", 10), wrap="word")
        self.log.pack(fill="both", expand=True, pady=(8, 0))

    def _connect(self) -> None:
        try:
            STATE.serial.open(self.port_var.get(), BAUDRATE)
            STATE.serial.start_reader(self._on_serial_line, self._on_serial_error)
            STATE.post_gui("log", f"Connected {self.port_var.get()} @ {BAUDRATE}")
            STATE.send_board("AI=IDLE")
        except Exception as exc:
            messagebox.showerror(APP_TITLE, str(exc))

    def _disconnect(self) -> None:
        STATE.serial.close()
        STATE.post_gui("log", "Serial disconnected")

    def _on_serial_line(self, line: str) -> None:
        STATE.post_gui("log", f"Board -> PC  {line}")
        upper = line.strip().upper()
        if upper == "BTN=APPROVE":
            STATE.decide("allow", "PA0")
        elif upper == "BTN=DENY":
            STATE.decide("deny", "PA4")

    def _on_serial_error(self, text: str) -> None:
        STATE.post_gui("log", text)

    def _tick(self) -> None:
        while True:
            try:
                kind, payload = STATE.gui_queue.get_nowait()
            except queue.Empty:
                break
            self._apply_gui_event(kind, payload)
        self.after(80, self._tick)

    def _apply_gui_event(self, kind: str, payload: Any) -> None:
        if kind == "log":
            stamp = time.strftime("%H:%M:%S")
            self.log.insert("end", f"[{stamp}] {payload}\n")
            self.log.see("end")
        elif kind == "status":
            self.status_label.configure(text=str(payload))
        elif kind == "approval":
            pending: PendingApproval = payload
            self.pending_label.configure(text=f"#{pending.request_id} {pending.tool_name}: {pending.summary}")
        elif kind == "approval_done":
            self.pending_label.configure(text=f"Decision: {payload['decision']} from {payload['source']}")

    def _on_close(self) -> None:
        try:
            self.http_server.shutdown()
            STATE.serial.close()
        finally:
            self.destroy()


if __name__ == "__main__":
    MonitorApp().mainloop()
