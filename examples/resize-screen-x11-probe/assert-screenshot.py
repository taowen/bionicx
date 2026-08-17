#!/usr/bin/env python3
"""Require the after-resize fill color on the Android screenshot."""

import sys
from pathlib import Path

from PIL import Image


def main() -> int:
    path = Path(sys.argv[1])
    want = tuple(int(part) for part in sys.argv[2].split(","))
    image = Image.open(path).convert("RGB")
    width, height = image.size
    matched = 0
    sampled = 0
    for y in range(0, height, 4):
        for x in range(0, width, 4):
            sampled += 1
            pixel = image.getpixel((x, y))
            exact = all(abs(pixel[i] - want[i]) <= 24 for i in range(3))
            blue = pixel[2] > 160 and pixel[2] > pixel[0] + 40
            if exact or blue:
                matched += 1
    if matched < 80:
        print(f"resize-screen screenshot missed {want}: "
              f"matched={matched} sampled={sampled} size={width}x{height}",
              file=sys.stderr)
        return 1
    print(f"resize-screen screenshot matched {want} count={matched}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
