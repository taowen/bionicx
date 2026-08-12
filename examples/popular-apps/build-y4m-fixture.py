#!/usr/bin/env python3
"""Build a deterministic YUV4MPEG2 animation without a media toolchain."""

import pathlib
import sys


WIDTH = 320
HEIGHT = 180
FRAMES = 90


def main() -> None:
    destination = pathlib.Path(sys.argv[1])
    raw_destination = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None
    destination.parent.mkdir(parents=True, exist_ok=True)
    if raw_destination:
        raw_destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("wb") as output, (
        raw_destination.open("wb") if raw_destination else open("/dev/null", "wb")
    ) as raw_output:
        output.write(
            f"YUV4MPEG2 W{WIDTH} H{HEIGHT} F30:1 Ip A1:1 C420jpeg XYSCSS=420JPEG\n".encode()
        )
        for frame in range(FRAMES):
            output.write(b"FRAME\n")
            frame_data = bytearray()
            for y in range(HEIGHT):
                for x in range(WIDTH):
                    bar = ((x + frame * 4) // 40) % 4
                    luminance = (38, 92, 158, 220)[bar]
                    if 62 < y < 118 and 42 < x < 278:
                        luminance = min(235, luminance + 18)
                    frame_data.append(luminance)
            chroma_width = WIDTH // 2
            chroma_height = HEIGHT // 2
            for plane_offset in (0, 37):
                for y in range(chroma_height):
                    row = bytes(
                        96 + ((x // 20 + frame // 8 + plane_offset) % 4) * 20
                        for x in range(chroma_width)
                    )
                    frame_data.extend(row)
            output.write(frame_data)
            raw_output.write(frame_data)


if __name__ == "__main__":
    main()
