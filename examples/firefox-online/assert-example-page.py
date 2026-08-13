#!/usr/bin/env python3
"""Reject a black frame or empty chrome; require a light navigated page."""

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
    light = dark = 0
    for row in rows:
        for x in range(width):
            r, g, b = row[x * channels], row[x * channels + 1], row[x * channels + 2]
            luma = (r * 30 + g * 59 + b * 11) // 100
            if luma >= 160:
                light += 1
            if luma <= 24:
                dark += 1
    total = width * height
    light_frac = light / total
    dark_frac = dark / total
    # Example Domain is a light document in Firefox chrome; a failed NSS
    # splash or empty compositor is almost all black.
    ok = light_frac >= 0.20 and 0.02 <= dark_frac <= 0.70
    status = "PASS" if ok else "FAIL"
    print(
        f"BXTEST {status} firefox-example-page "
        f"size={width}x{height} light={light_frac:.3f} dark={dark_frac:.3f}"
    )
    raise SystemExit(0 if ok else 1)


if __name__ == "__main__":
    main()
