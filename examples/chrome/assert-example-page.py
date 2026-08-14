#!/usr/bin/env python3
"""Require a painted Baidu homepage, not about:blank or chrome://gpu."""

from pathlib import Path
import struct
import sys
import zlib


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png(path: Path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"not a PNG: {path}")
    offset = 8
    width = height = bit_depth = color_type = None
    idat = bytearray()
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        name = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + length]
        offset += 12 + length
        if name == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(
                ">IIBB", chunk[:10])
        elif name == b"IDAT":
            idat.extend(chunk)
        elif name == b"IEND":
            break
    if width is None or not idat or bit_depth != 8 or color_type not in (2, 6):
        raise SystemExit(f"unsupported PNG: {path}")
    channels = 4 if color_type == 6 else 3
    raw = zlib.decompress(bytes(idat))
    stride = 1 + width * channels
    prev = bytearray(width * channels)
    rows = []
    for y in range(height):
        filt = raw[y * stride]
        scan = raw[y * stride + 1:(y + 1) * stride]
        recon = bytearray(width * channels)
        for i in range(width * channels):
            left = recon[i - channels] if i >= channels else 0
            up = prev[i]
            up_left = prev[i - channels] if i >= channels else 0
            x = scan[i]
            if filt == 0:
                recon[i] = x
            elif filt == 1:
                recon[i] = (x + left) & 255
            elif filt == 2:
                recon[i] = (x + up) & 255
            elif filt == 3:
                recon[i] = (x + (left + up) // 2) & 255
            elif filt == 4:
                recon[i] = (x + paeth(left, up, up_left)) & 255
            else:
                raise SystemExit(f"unknown PNG filter {filt}")
        prev = recon
        rows.append(recon)
    return width, height, rows, channels


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} SCREENSHOT.png")
    path = Path(sys.argv[1])
    width, height, rows, channels = read_png(path)
    # Chrome is 1920x1080 on the left; vivo screenshots include a black
    # bezel. Score the document, not the bezel or the toolbar.
    doc_right = min(width, 1920)
    doc_top = int(height * 0.18)
    doc_bottom = int(height * 0.72)
    light = red = total = 0
    for y in range(doc_top, doc_bottom):
        row = rows[y]
        for x in range(doc_right):
            r = row[x * channels]
            g = row[x * channels + 1]
            b = row[x * channels + 2]
            luma = (r * 30 + g * 59 + b * 11) // 100
            total += 1
            if luma >= 200:
                light += 1
            if r >= 180 and r > g + 40 and r > b + 40:
                red += 1
    light_frac = light / total
    red_frac = red / total
    # about:blank is white with no brand red. chrome://gpu is a gray
    # table. Baidu's current homepage is a light page with a red logo.
    ok = light_frac >= 0.70 and red_frac >= 0.002
    status = "PASS" if ok else "FAIL"
    print(
        f"BXTEST {status} chrome-baidu-page "
        f"size={width}x{height} light={light_frac:.3f} "
        f"red={red_frac:.4f}"
    )
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()
