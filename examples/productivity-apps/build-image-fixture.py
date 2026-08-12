#!/usr/bin/env python3
"""Write a deterministic 640x480 binary PPM for the GIMP workflow."""

import pathlib
import sys


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} OUTPUT.ppm")
    width, height = 640, 480
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            # A four-quadrant base plus a high-contrast circle makes edits and
            # exports easy to recognize without an image-library dependency.
            if (x - 480) ** 2 + (y - 120) ** 2 < 72 ** 2:
                rgb = (251, 191, 36)
            elif x < width // 2 and y < height // 2:
                rgb = (23, 37, 84)
            elif x >= width // 2 and y < height // 2:
                rgb = (15, 118, 110)
            elif x < width // 2:
                rgb = (94, 234, 212)
            else:
                rgb = (30, 64, 175)
            pixels.extend(rgb)
    output = pathlib.Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixels)


if __name__ == "__main__":
    main()
