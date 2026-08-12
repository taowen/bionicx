#!/usr/bin/env python3
"""Assert that the host-GPU probe reached Android's final compositor."""

import struct
import sys
import zlib
from pathlib import Path


def read_png(path: Path):
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("not a PNG")

    position = 8
    compressed = bytearray()
    width = height = color_type = bit_depth = interlace = None
    while position < len(data):
        length = struct.unpack_from(">I", data, position)[0]
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + length]
        position += 12 + length
        if kind == b"IHDR":
            width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
        elif kind == b"IDAT":
            compressed.extend(payload)
        elif kind == b"IEND":
            break

    if bit_depth != 8 or color_type not in (2, 6) or interlace != 0:
        raise ValueError(
            f"unsupported PNG format depth={bit_depth} color={color_type} "
            f"interlace={interlace}"
        )
    channels = 3 if color_type == 2 else 4
    stride = width * channels
    raw = zlib.decompress(compressed)
    rows = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        filter_type = raw[offset]
        encoded = raw[offset + 1:offset + 1 + stride]
        offset += stride + 1
        row = bytearray(stride)
        for index, value in enumerate(encoded):
            left = row[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                estimate = left + above - upper_left
                distances = (abs(estimate - left), abs(estimate - above),
                             abs(estimate - upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                raise ValueError(f"unsupported PNG filter {filter_type}")
            row[index] = (value + predictor) & 0xff
        rows.append(row)
        previous = row
    return width, height, channels, rows


def main() -> int:
    width, height, channels, rows = read_png(Path(sys.argv[1]))
    if width < 1001 or height < 701:
        raise ValueError(f"screenshot too small: {width}x{height}")

    def rgb(x, y):
        start = x * channels
        return tuple(rows[y][start:start + 3])

    blue = rgb(100, 100)
    red = rgb(440, 320)
    outside = rgb(1000, 700)
    passed = (blue[0] < 80 and blue[1] < 100 and blue[2] > 140
              and red[0] > 180 and red[1] < 100 and red[2] < 100
              and max(outside) < 40)
    state = "PASS" if passed else "FAIL"
    print(f"BXTEST {state} host-gl-compositor blue={blue} red={red} "
          f"outside={outside} size={width}x{height}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
