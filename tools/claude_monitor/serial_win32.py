"""Small Win32 serial helper without third-party dependencies."""

from __future__ import annotations

import ctypes
from ctypes import wintypes
import threading
import time
from typing import Callable, Optional


GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x80
INVALID_HANDLE_VALUE = wintypes.HANDLE(-1).value


class DCB(ctypes.Structure):
    _fields_ = [
        ("DCBlength", wintypes.DWORD),
        ("BaudRate", wintypes.DWORD),
        ("flags", wintypes.DWORD),
        ("wReserved", wintypes.WORD),
        ("XonLim", wintypes.WORD),
        ("XoffLim", wintypes.WORD),
        ("ByteSize", wintypes.BYTE),
        ("Parity", wintypes.BYTE),
        ("StopBits", wintypes.BYTE),
        ("XonChar", ctypes.c_char),
        ("XoffChar", ctypes.c_char),
        ("ErrorChar", ctypes.c_char),
        ("EofChar", ctypes.c_char),
        ("EvtChar", ctypes.c_char),
        ("wReserved1", wintypes.WORD),
    ]


class COMMTIMEOUTS(ctypes.Structure):
    _fields_ = [
        ("ReadIntervalTimeout", wintypes.DWORD),
        ("ReadTotalTimeoutMultiplier", wintypes.DWORD),
        ("ReadTotalTimeoutConstant", wintypes.DWORD),
        ("WriteTotalTimeoutMultiplier", wintypes.DWORD),
        ("WriteTotalTimeoutConstant", wintypes.DWORD),
    ]


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

kernel32.CreateFileW.argtypes = [
    wintypes.LPCWSTR,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.LPVOID,
    wintypes.DWORD,
    wintypes.DWORD,
    wintypes.HANDLE,
]
kernel32.CreateFileW.restype = wintypes.HANDLE
kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
kernel32.CloseHandle.restype = wintypes.BOOL
kernel32.GetCommState.argtypes = [wintypes.HANDLE, ctypes.POINTER(DCB)]
kernel32.GetCommState.restype = wintypes.BOOL
kernel32.SetCommState.argtypes = [wintypes.HANDLE, ctypes.POINTER(DCB)]
kernel32.SetCommState.restype = wintypes.BOOL
kernel32.SetCommTimeouts.argtypes = [wintypes.HANDLE, ctypes.POINTER(COMMTIMEOUTS)]
kernel32.SetCommTimeouts.restype = wintypes.BOOL
kernel32.ReadFile.argtypes = [
    wintypes.HANDLE,
    wintypes.LPVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.LPVOID,
]
kernel32.ReadFile.restype = wintypes.BOOL
kernel32.WriteFile.argtypes = [
    wintypes.HANDLE,
    wintypes.LPCVOID,
    wintypes.DWORD,
    ctypes.POINTER(wintypes.DWORD),
    wintypes.LPVOID,
]
kernel32.WriteFile.restype = wintypes.BOOL


def _raise_last_error(prefix: str) -> None:
    err = ctypes.get_last_error()
    raise OSError(err, f"{prefix}: Win32 error {err}")


class Win32Serial:
    def __init__(self) -> None:
        self._handle: Optional[int] = None
        self._lock = threading.Lock()
        self._rx_thread: Optional[threading.Thread] = None
        self._running = threading.Event()

    @property
    def connected(self) -> bool:
        return self._handle is not None

    def open(self, port: str, baudrate: int = 115200) -> None:
        self.close()
        name = port.strip().upper()
        if not name.startswith("\\\\.\\"):
            name = "\\\\.\\" + name

        handle = kernel32.CreateFileW(
            name,
            GENERIC_READ | GENERIC_WRITE,
            0,
            None,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            None,
        )
        if handle == INVALID_HANDLE_VALUE:
            _raise_last_error(f"open {port}")

        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        if not kernel32.GetCommState(handle, ctypes.byref(dcb)):
            kernel32.CloseHandle(handle)
            _raise_last_error("GetCommState")
        dcb.BaudRate = int(baudrate)
        dcb.ByteSize = 8
        dcb.Parity = 0
        dcb.StopBits = 0
        dcb.flags = 0x00000001
        if not kernel32.SetCommState(handle, ctypes.byref(dcb)):
            kernel32.CloseHandle(handle)
            _raise_last_error("SetCommState")

        timeouts = COMMTIMEOUTS(50, 0, 50, 0, 100)
        if not kernel32.SetCommTimeouts(handle, ctypes.byref(timeouts)):
            kernel32.CloseHandle(handle)
            _raise_last_error("SetCommTimeouts")

        self._handle = handle

    def close(self) -> None:
        self.stop_reader()
        with self._lock:
            if self._handle is not None:
                kernel32.CloseHandle(self._handle)
                self._handle = None

    def write_line(self, line: str) -> None:
        text = line.rstrip("\r\n") + "\r\n"
        data = text.encode("ascii", errors="replace")
        written = wintypes.DWORD(0)
        with self._lock:
            if self._handle is None:
                raise RuntimeError("serial port is not open")
            ok = kernel32.WriteFile(self._handle, data, len(data), ctypes.byref(written), None)
        if not ok:
            _raise_last_error("WriteFile")

    def start_reader(self, on_line: Callable[[str], None], on_error: Callable[[str], None]) -> None:
        if self._handle is None:
            raise RuntimeError("serial port is not open")
        self.stop_reader()
        self._running.set()
        self._rx_thread = threading.Thread(
            target=self._read_loop,
            args=(on_line, on_error),
            name="serial-reader",
            daemon=True,
        )
        self._rx_thread.start()

    def stop_reader(self) -> None:
        self._running.clear()
        if self._rx_thread and self._rx_thread.is_alive():
            self._rx_thread.join(timeout=0.5)
        self._rx_thread = None

    def _read_loop(self, on_line: Callable[[str], None], on_error: Callable[[str], None]) -> None:
        buf = bytearray()
        chunk = ctypes.create_string_buffer(64)
        while self._running.is_set():
            with self._lock:
                handle = self._handle
            if handle is None:
                break
            count = wintypes.DWORD(0)
            ok = kernel32.ReadFile(handle, chunk, 64, ctypes.byref(count), None)
            if not ok:
                on_error(f"ReadFile failed: {ctypes.get_last_error()}")
                time.sleep(0.2)
                continue
            if count.value == 0:
                continue
            buf.extend(chunk.raw[: count.value])
            while b"\n" in buf:
                raw, _, rest = buf.partition(b"\n")
                buf = bytearray(rest)
                line = raw.rstrip(b"\r").decode("ascii", errors="replace")
                if line:
                    on_line(line)
