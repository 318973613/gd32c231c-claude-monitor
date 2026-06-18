import struct
from pathlib import Path

W = 240
H = 320

TYPE_RLE_RGB565 = 1
TYPE_TEXT = 2
FMT_RGB565_RLE16 = 1
FMT_ASCII = 2

ASSET_BG = 1
ASSET_TITLE = 2
ASSET_HINT = 3
ASSET_FOOTER = 4


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


COLORS = {
    "bg": rgb565(6, 6, 5),
    "grid": rgb565(34, 23, 18),
    "panel": rgb565(24, 18, 15),
    "panel2": rgb565(38, 26, 20),
    "line": rgb565(112, 66, 42),
    "orange": rgb565(217, 119, 87),
    "orange2": rgb565(255, 176, 132),
    "green": rgb565(123, 224, 163),
    "ink": rgb565(255, 242, 232),
}


def rect(pixels, x0, y0, x1, y1, color):
    x0 = max(0, min(W, x0))
    x1 = max(0, min(W, x1))
    y0 = max(0, min(H, y0))
    y1 = max(0, min(H, y1))
    for y in range(y0, y1):
        row = pixels[y]
        for x in range(x0, x1):
            row[x] = color


def make_background():
    pixels = [[COLORS["bg"] for _ in range(W)] for _ in range(H)]

    for y in range(0, H, 16):
        rect(pixels, 0, y, W, y + 1, COLORS["grid"])
    for x in range(0, W, 24):
        rect(pixels, x, 0, x + 1, H, COLORS["grid"])

    rect(pixels, 0, 0, W, 5, COLORS["line"])
    rect(pixels, 0, 0, 5, H, COLORS["line"])
    rect(pixels, W - 5, 0, W, H, COLORS["line"])
    rect(pixels, 0, H - 5, W, H, COLORS["line"])

    rect(pixels, 16, 18, 224, 82, COLORS["panel"])
    rect(pixels, 20, 22, 220, 26, COLORS["orange"])
    rect(pixels, 20, 74, 220, 78, COLORS["line"])

    rect(pixels, 22, 196, 218, 286, COLORS["panel"])
    rect(pixels, 28, 204, 212, 208, COLORS["line"])
    rect(pixels, 28, 254, 212, 258, COLORS["line"])

    rect(pixels, 42, 112, 198, 174, COLORS["orange"])
    rect(pixels, 30, 126, 210, 154, COLORS["orange"])
    rect(pixels, 54, 174, 67, 194, COLORS["orange"])
    rect(pixels, 81, 174, 94, 194, COLORS["orange"])
    rect(pixels, 146, 174, 159, 194, COLORS["orange"])
    rect(pixels, 173, 174, 186, 194, COLORS["orange"])
    rect(pixels, 68, 126, 78, 140, COLORS["bg"])
    rect(pixels, 162, 126, 172, 140, COLORS["bg"])
    rect(pixels, 48, 160, 192, 166, COLORS["orange2"])

    rect(pixels, 36, 214, 92, 246, COLORS["panel2"])
    rect(pixels, 100, 214, 204, 246, COLORS["panel2"])
    rect(pixels, 36, 264, 204, 278, COLORS["panel2"])
    rect(pixels, 42, 220, 86, 225, COLORS["green"])
    rect(pixels, 106, 220, 198, 225, COLORS["orange2"])
    rect(pixels, 42, 269, 198, 273, COLORS["line"])

    return pixels


def rle_rgb565(pixels):
    flat = [px for row in pixels for px in row]
    out = bytearray()
    i = 0
    while i < len(flat):
        color = flat[i]
        run = 1
        while i + run < len(flat) and flat[i + run] == color and run < 65535:
            run += 1
        out += struct.pack("<HH", run, color)
        i += run
    return bytes(out)


def make_blob():
    assets = [
        (ASSET_BG, TYPE_RLE_RGB565, FMT_RGB565_RLE16, W, H, rle_rgb565(make_background())),
        (ASSET_TITLE, TYPE_TEXT, FMT_ASCII, 0, 0, b"CLAUDE DESK\0"),
        (ASSET_HINT, TYPE_TEXT, FMT_ASCII, 0, 0, b"TIME=HH:MM:SS  USART0\0"),
        (ASSET_FOOTER, TYPE_TEXT, FMT_ASCII, 0, 0, b"GD25Q16 EXTERNAL ASSET PACK\0"),
    ]

    header_size = 20 + len(assets) * 20
    offset = header_size
    entries = []
    payload = bytearray()
    for asset_id, asset_type, fmt, width, height, data in assets:
        entries.append((asset_id, asset_type, offset, len(data), width, height, fmt, 0))
        payload += data
        offset += len(data)

    total_size = header_size + len(payload)
    header = bytearray()
    header += b"CUI1"
    header += struct.pack("<HHIHHI", 1, header_size, total_size, len(assets), 0, 0)
    for e in entries:
        header += struct.pack("<HHIIHHHH", *e)

    blob = header + payload
    checksum = sum(blob[20:]) & 0xFFFFFFFF
    blob[16:20] = struct.pack("<I", checksum)
    return bytes(blob)


def write_c_array(blob, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    h = out_dir / "ui_assets.h"
    c = out_dir / "ui_assets.c"

    h.write_text(
        "#ifndef UI_ASSETS_H\n"
        "#define UI_ASSETS_H\n\n"
        "#include <stdint.h>\n\n"
        "extern const uint8_t ui_asset_blob[];\n"
        "extern const uint32_t ui_asset_blob_size;\n\n"
        "#endif /* UI_ASSETS_H */\n",
        encoding="ascii",
    )

    lines = [
        '#include "ui_assets.h"',
        "",
        f"const uint32_t ui_asset_blob_size = {len(blob)}U;",
        "const uint8_t ui_asset_blob[] = {",
    ]
    for i in range(0, len(blob), 12):
        chunk = blob[i : i + 12]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    c.write_text("\n".join(lines) + "\n", encoding="ascii")

    (out_dir / "ui_assets.bin").write_bytes(blob)
    print(f"asset blob: {len(blob)} bytes")


if __name__ == "__main__":
    write_c_array(make_blob(), Path(r"D:\gd32c231c\project\07_SPI_FLASH_AssetWriter\Assets"))
