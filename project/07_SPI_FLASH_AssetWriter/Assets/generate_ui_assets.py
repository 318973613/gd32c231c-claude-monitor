#!/usr/bin/env python3
"""Generate the external flash UI asset package.

The runtime reads a compact custom package from GD25Q16:
  - 20-byte package header
  - 20-byte asset table entries
  - RGB565 RLE background
  - small ASCII text resources
"""

from __future__ import annotations

import struct
from pathlib import Path


WIDTH = 240
HEIGHT = 320
ASSET_MAGIC = b"CUI1"
VERSION = 1
ASSET_TYPE_RLE_RGB565 = 1
ASSET_TYPE_TEXT = 2
FORMAT_RGB565 = 1
FORMAT_ASCII = 2

ROOT = Path(__file__).resolve().parent
BIN_PATH = ROOT / "ui_assets.bin"
C_PATH = ROOT / "ui_assets.c"
PREVIEW_PATH = ROOT / "ui_assets_preview.bmp"


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


PALETTE = {
    "bg": rgb565(5, 9, 13),
    "bg2": rgb565(8, 17, 24),
    "grid": rgb565(27, 47, 58),
    "grid_hi": rgb565(46, 83, 93),
    "panel": rgb565(14, 31, 39),
    "panel2": rgb565(23, 49, 56),
    "panel3": rgb565(34, 68, 71),
    "ink": rgb565(231, 239, 228),
    "muted": rgb565(116, 140, 137),
    "amber": rgb565(246, 174, 124),
    "amber2": rgb565(181, 100, 75),
    "cyan": rgb565(101, 245, 232),
    "green": rgb565(111, 244, 184),
    "red": rgb565(244, 89, 118),
    "magenta": rgb565(229, 98, 151),
    "shadow": rgb565(2, 5, 8),
}


FONT_3X5 = {
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"),
    "7": ("111", "001", "010", "010", "010"),
    "8": ("111", "101", "111", "101", "111"),
    "9": ("111", "101", "111", "001", "111"),
    "A": ("010", "101", "111", "101", "101"),
    "B": ("110", "101", "110", "101", "110"),
    "C": ("111", "100", "100", "100", "111"),
    "D": ("110", "101", "101", "101", "110"),
    "E": ("111", "100", "110", "100", "111"),
    "F": ("111", "100", "110", "100", "100"),
    "G": ("111", "100", "101", "101", "111"),
    "H": ("101", "101", "111", "101", "101"),
    "I": ("111", "010", "010", "010", "111"),
    "L": ("100", "100", "100", "100", "111"),
    "M": ("101", "111", "111", "101", "101"),
    "O": ("111", "101", "101", "101", "111"),
    "P": ("111", "101", "111", "100", "100"),
    "R": ("110", "101", "110", "101", "101"),
    "S": ("111", "100", "111", "001", "111"),
    "T": ("111", "010", "010", "010", "010"),
    "U": ("101", "101", "101", "101", "111"),
    "V": ("101", "101", "101", "101", "010"),
    "X": ("101", "101", "010", "101", "101"),
    " ": ("000", "000", "000", "000", "000"),
    ":": ("000", "010", "000", "010", "000"),
    ".": ("000", "000", "000", "000", "010"),
    "-": ("000", "000", "111", "000", "000"),
}


def new_canvas() -> list[int]:
    return [PALETTE["bg"]] * (WIDTH * HEIGHT)


def put(canvas: list[int], x: int, y: int, color: int) -> None:
    if 0 <= x < WIDTH and 0 <= y < HEIGHT:
        canvas[y * WIDTH + x] = color


def rect(canvas: list[int], x: int, y: int, w: int, h: int, color: int) -> None:
    for yy in range(max(0, y), min(HEIGHT, y + h)):
        start = yy * WIDTH + max(0, x)
        end = yy * WIDTH + min(WIDTH, x + w)
        canvas[start:end] = [color] * (end - start)


def frame(canvas: list[int], x: int, y: int, w: int, h: int, color: int, t: int = 1) -> None:
    rect(canvas, x, y, w, t, color)
    rect(canvas, x, y + h - t, w, t, color)
    rect(canvas, x, y, t, h, color)
    rect(canvas, x + w - t, y, t, h, color)


def text(canvas: list[int], x: int, y: int, value: str, color: int, scale: int = 1) -> None:
    cursor = x
    for ch in value.upper():
        glyph = FONT_3X5.get(ch, FONT_3X5[" "])
        for row, bits in enumerate(glyph):
            for col, bit in enumerate(bits):
                if bit == "1":
                    rect(canvas, cursor + col * scale, y + row * scale, scale, scale, color)
        cursor += 4 * scale


def line_h(canvas: list[int], x: int, y: int, w: int, color: int) -> None:
    rect(canvas, x, y, w, 1, color)


def line_v(canvas: list[int], x: int, y: int, h: int, color: int) -> None:
    rect(canvas, x, y, 1, h, color)


def draw_background() -> list[int]:
    c = new_canvas()

    for y in range(HEIGHT):
        if y % 5 == 0:
            shade = PALETTE["bg2"]
            rect(c, 0, y, WIDTH, 1, shade)

    for x in range(16, WIDTH, 16):
        line_v(c, x, 18, HEIGHT - 36, PALETTE["grid"])
    for y in range(24, HEIGHT - 20, 16):
        line_h(c, 10, y, WIDTH - 20, PALETTE["grid"])
    for x in range(32, WIDTH, 64):
        line_v(c, x, 18, HEIGHT - 36, PALETTE["grid_hi"])
    for y in range(48, HEIGHT - 20, 64):
        line_h(c, 10, y, WIDTH - 20, PALETTE["grid_hi"])

    frame(c, 10, 10, 220, 300, PALETTE["amber"], 2)
    frame(c, 14, 14, 212, 292, PALETTE["panel3"], 1)
    rect(c, 16, 16, 208, 290, PALETTE["shadow"])
    frame(c, 17, 17, 206, 288, PALETTE["grid_hi"], 1)

    rect(c, 22, 25, 196, 50, PALETTE["panel"])
    frame(c, 22, 25, 196, 50, PALETTE["amber2"], 1)
    rect(c, 28, 33, 184, 3, PALETTE["amber"])
    rect(c, 28, 38, 184, 4, PALETTE["cyan"])
    text(c, 32, 19, "GD32 C231C", PALETTE["muted"], 1)
    text(c, 151, 19, "FLASH UI", PALETTE["green"], 1)

    rect(c, 49, 96, 142, 55, PALETTE["amber"])
    rect(c, 57, 104, 126, 39, rgb565(252, 187, 138))
    rect(c, 75, 111, 14, 18, PALETTE["shadow"])
    rect(c, 151, 111, 14, 18, PALETTE["shadow"])
    rect(c, 46, 128, 12, 23, PALETTE["amber"])
    rect(c, 182, 128, 12, 23, PALETTE["amber"])
    rect(c, 63, 151, 12, 27, PALETTE["amber"])
    rect(c, 101, 151, 12, 27, PALETTE["amber"])
    rect(c, 127, 151, 12, 27, PALETTE["amber"])
    rect(c, 165, 151, 12, 27, PALETTE["amber"])
    rect(c, 60, 139, 120, 6, PALETTE["ink"])
    rect(c, 60, 145, 120, 3, PALETTE["cyan"])
    rect(c, 72, 90, 96, 3, PALETTE["cyan"])
    rect(c, 88, 84, 64, 2, PALETTE["amber"])
    text(c, 93, 156, "CLAUDE", PALETTE["muted"], 1)

    rect(c, 24, 201, 192, 84, PALETTE["panel"])
    frame(c, 24, 201, 192, 84, PALETTE["panel3"], 2)
    rect(c, 31, 211, 56, 9, PALETTE["cyan"])
    rect(c, 106, 211, 76, 9, PALETTE["amber"])
    text(c, 36, 215, "TEMP", PALETTE["ink"], 1)
    text(c, 113, 215, "HUM", PALETTE["ink"], 1)

    rect(c, 34, 224, 62, 29, PALETTE["panel2"])
    rect(c, 106, 224, 84, 29, PALETTE["panel2"])
    rect(c, 194, 229, 12, 12, PALETTE["green"])
    frame(c, 34, 224, 62, 29, PALETTE["grid_hi"], 1)
    frame(c, 106, 224, 84, 29, PALETTE["grid_hi"], 1)
    frame(c, 192, 227, 16, 16, PALETTE["grid_hi"], 1)

    rect(c, 36, 263, 48, 15, PALETTE["panel2"])
    rect(c, 92, 263, 48, 15, PALETTE["panel2"])
    rect(c, 148, 263, 48, 15, PALETTE["panel2"])
    text(c, 39, 256, "SMP", PALETTE["green"], 1)
    text(c, 95, 256, "ERR", PALETTE["red"], 1)
    text(c, 151, 256, "RX", PALETTE["amber"], 1)

    for i in range(7):
        x = 28 + i * 27
        rect(c, x, 291, 15, 2, PALETTE["grid_hi"])
        rect(c, x + 5, 295, 10, 1, PALETTE["cyan"] if i % 2 == 0 else PALETTE["amber"])

    return c


def encode_rle_rgb565(canvas: list[int]) -> bytes:
    out = bytearray()
    i = 0
    while i < len(canvas):
        color = canvas[i]
        run = 1
        i += 1
        while i < len(canvas) and canvas[i] == color and run < 0xFFFF:
            run += 1
            i += 1
        out += struct.pack("<HH", run, color)
    return bytes(out)


def make_package() -> bytes:
    background = encode_rle_rgb565(draw_background())
    assets = [
        (1, ASSET_TYPE_RLE_RGB565, background, WIDTH, HEIGHT, FORMAT_RGB565),
        (2, ASSET_TYPE_TEXT, b"CLAUDE CORE\0", 0, 0, FORMAT_ASCII),
        (3, ASSET_TYPE_TEXT, b"GD32C231C FLASH UI\0", 0, 0, FORMAT_ASCII),
        (4, ASSET_TYPE_TEXT, b"TIME TEMP HUM SAMPLE ERR RX\0", 0, 0, FORMAT_ASCII),
    ]

    header_size = 20 + len(assets) * 20
    offset = header_size
    entries = bytearray()
    payload = bytearray()
    for asset_id, asset_type, data, width, height, fmt in assets:
        entries += struct.pack("<HHIIHHHH", asset_id, asset_type, offset, len(data), width, height, fmt, 0)
        payload += data
        offset += len(data)

    total_size = header_size + len(payload)
    checksum = sum(entries + payload) & 0xFFFFFFFF
    header = struct.pack("<4sHHIHHI", ASSET_MAGIC, VERSION, header_size, total_size, len(assets), 0, checksum)
    return header + entries + payload


def write_c(blob: bytes) -> None:
    lines = [
        '#include "ui_assets.h"',
        "",
        f"const uint32_t ui_asset_blob_size = {len(blob)}U;",
        "const uint8_t ui_asset_blob[] = {",
    ]
    for i in range(0, len(blob), 12):
        chunk = ", ".join(f"0x{b:02X}" for b in blob[i : i + 12])
        lines.append(f"    {chunk},")
    lines.append("};")
    lines.append("")
    C_PATH.write_text("\n".join(lines), encoding="ascii")


def write_preview(canvas: list[int]) -> None:
    row_size = ((WIDTH * 3 + 3) // 4) * 4
    pixel_data_size = row_size * HEIGHT
    file_size = 14 + 40 + pixel_data_size
    out = bytearray()
    out += struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 54)
    out += struct.pack("<IIIHHIIIIII", 40, WIDTH, HEIGHT, 1, 24, 0, pixel_data_size, 2835, 2835, 0, 0)
    for y in range(HEIGHT - 1, -1, -1):
        row = bytearray()
        for x in range(WIDTH):
            color = canvas[y * WIDTH + x]
            r = ((color >> 11) & 0x1F) * 255 // 31
            g = ((color >> 5) & 0x3F) * 255 // 63
            b = (color & 0x1F) * 255 // 31
            row += bytes((b, g, r))
        row += b"\x00" * (row_size - len(row))
        out += row
    PREVIEW_PATH.write_bytes(out)


def main() -> None:
    canvas = draw_background()
    blob = make_package()
    BIN_PATH.write_bytes(blob)
    write_c(blob)
    write_preview(canvas)
    print(f"Wrote {BIN_PATH} ({len(blob)} bytes)")
    print(f"Wrote {C_PATH}")
    print(f"Wrote {PREVIEW_PATH}")


if __name__ == "__main__":
    main()
