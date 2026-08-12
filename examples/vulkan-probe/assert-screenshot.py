#!/usr/bin/env python3
"""Assert that the Vulkan AHardwareBuffer reached Android's compositor."""

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
                predictor = (left, above, upper_left)[distances.index(
                    min(distances)
                )]
            else:
                raise ValueError(f"unsupported PNG filter {filter_type}")
            row[index] = (value + predictor) & 0xff
        rows.append(row)
        previous = row
    return width, height, channels, rows


def main() -> int:
    width, height, channels, rows = read_png(Path(sys.argv[1]))
    if width < 1000 or height < 700:
        raise ValueError(f"screenshot too small: {width}x{height}")

    matching = []
    triangle = []
    for y, row in enumerate(rows):
        for x in range(width):
            offset = x * channels
            red, green, blue = row[offset:offset + 3]
            if (15 <= red <= 40 and 175 <= green <= 205
                    and 45 <= blue <= 85
                    and green - red >= 140 and green - blue >= 100):
                matching.append((x, y))
            if (205 <= red <= 245 and 5 <= green <= 40 and 0 <= blue <= 30
                    and red - green >= 175 and red - blue >= 190):
                triangle.append((x, y))

    if matching:
        left = min(point[0] for point in matching)
        top = min(point[1] for point in matching)
        right = max(point[0] for point in matching)
        bottom = max(point[1] for point in matching)
    else:
        left = top = right = bottom = -1
    if triangle:
        triangle_left = min(point[0] for point in triangle)
        triangle_top = min(point[1] for point in triangle)
        triangle_right = max(point[0] for point in triangle)
        triangle_bottom = max(point[1] for point in triangle)
    else:
        triangle_left = triangle_top = triangle_right = triangle_bottom = -1
    passed = (len(matching) > 150_000
              and right - left > 600 and bottom - top > 340
              and len(triangle) > 30_000
              and triangle_right - triangle_left > 400
              and triangle_bottom - triangle_top > 200)
    state = "PASS" if passed else "FAIL"
    print(f"BXTEST {state} host-vulkan-compositor pixels={len(matching)} "
          f"bounds={left},{top}-{right},{bottom} triangle={len(triangle)} "
          f"triangleBounds={triangle_left},{triangle_top}-"
          f"{triangle_right},{triangle_bottom} size={width}x{height}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
