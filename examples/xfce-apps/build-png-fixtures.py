#!/usr/bin/env python3
import binascii
import pathlib
import struct
import sys
import zlib


def chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(
        ">I", binascii.crc32(body) & 0xFFFFFFFF
    )


def write_png(path: pathlib.Path, first: tuple[int, int, int],
              second: tuple[int, int, int]) -> None:
    width, height = 960, 540
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            diagonal = (x + y * 2) % 420 < 210
            color = first if diagonal else second
            if 90 < x < 870 and 80 < y < 460:
                color = tuple(min(255, channel + 35) for channel in color)
            rows.extend(color)
    signature = b"\x89PNG\r\n\x1a\n"
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.write_bytes(signature + chunk(b"IHDR", header)
                     + chunk(b"IDAT", zlib.compress(rows, 9))
                     + chunk(b"IEND", b""))


destination = pathlib.Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
write_png(destination / "bionicx-blue.png", (19, 60, 115), (38, 116, 189))
write_png(destination / "bionicx-orange.png", (112, 45, 10), (224, 103, 24))
