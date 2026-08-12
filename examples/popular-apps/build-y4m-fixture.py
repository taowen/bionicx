#!/usr/bin/env python3
"""Build a deterministic YUV4MPEG2 animation without a media toolchain."""

import pathlib
import struct
import sys
import wave


WIDTH = 320
HEIGHT = 180
FRAMES = 90


def riff_chunk(tag: bytes, payload: bytes) -> bytes:
    return tag + struct.pack("<I", len(payload)) + payload + (b"\0" if len(payload) & 1 else b"")


def riff_list(kind: bytes, payload: bytes) -> bytes:
    return riff_chunk(b"LIST", kind + payload)


def build_avi(destination: pathlib.Path, video: bytes, audio: bytes) -> None:
    """Mux deterministic I420 video and stereo PCM into a small OpenDML-free AVI."""
    frame_size = WIDTH * HEIGHT * 3 // 2
    audio_frame_size = 4
    audio_per_video_frame = 48_000 // 30 * audio_frame_size
    avih = struct.pack(
        "<14I", 1_000_000 // 30, frame_size * 30 + 48_000 * audio_frame_size,
        0, 0x10, FRAMES, 0, 2, frame_size, WIDTH, HEIGHT, 0, 0, 0, 0
    )
    video_strh = struct.pack(
        "<4s4sIHH8I4h", b"vids", b"I420", 0, 0, 0, 0, 1, 30, 0,
        FRAMES, frame_size, 0xFFFFFFFF, 0, 0, 0, WIDTH, HEIGHT
    )
    video_strf = struct.pack(
        "<IiiHH4sIiiII", 40, WIDTH, HEIGHT, 1, 12, b"I420", frame_size,
        0, 0, 0, 0
    )
    audio_strh = struct.pack(
        "<4s4sIHH8I4h", b"auds", b"\0\0\0\0", 0, 0, 0, 0,
        audio_frame_size, 48_000 * audio_frame_size, 0,
        len(audio) // audio_frame_size, audio_per_video_frame,
        0xFFFFFFFF, audio_frame_size, 0, 0, 0, 0
    )
    audio_strf = struct.pack("<HHIIHH", 1, 2, 48_000, 48_000 * 4, 4, 16)
    hdrl = riff_list(
        b"hdrl",
        riff_chunk(b"avih", avih)
        + riff_list(b"strl", riff_chunk(b"strh", video_strh) + riff_chunk(b"strf", video_strf))
        + riff_list(b"strl", riff_chunk(b"strh", audio_strh) + riff_chunk(b"strf", audio_strf)),
    )

    movi_payload = bytearray()
    index = bytearray()
    offset = 4
    for frame in range(FRAMES):
        video_frame = video[frame * frame_size:(frame + 1) * frame_size]
        audio_frame = audio[
            frame * audio_per_video_frame:(frame + 1) * audio_per_video_frame
        ]
        for tag, payload, flags in (
            (b"00db", video_frame, 0x10),
            (b"01wb", audio_frame, 0x10),
        ):
            encoded = riff_chunk(tag, payload)
            index.extend(struct.pack("<4sIII", tag, flags, offset, len(payload)))
            movi_payload.extend(encoded)
            offset += len(encoded)
    body = b"AVI " + hdrl + riff_list(b"movi", bytes(movi_payload)) + riff_chunk(b"idx1", bytes(index))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(b"RIFF" + struct.pack("<I", len(body)) + body)


def main() -> None:
    destination = pathlib.Path(sys.argv[1])
    raw_destination = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else None
    wave_destination = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else None
    avi_destination = pathlib.Path(sys.argv[4]) if len(sys.argv) > 4 else None
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

    if wave_destination:
        wave_destination.parent.mkdir(parents=True, exist_ok=True)
        sample_rate = 48_000
        phase = 0
        phase_step = (440 << 32) // sample_rate
        with wave.open(str(wave_destination), "wb") as audio:
            audio.setnchannels(2)
            audio.setsampwidth(2)
            audio.setframerate(sample_rate)
            samples = bytearray()
            for _ in range(sample_rate * FRAMES // 30):
                sample = 5000 if phase & 0x80000000 else -5000
                phase = (phase + phase_step) & 0xFFFFFFFF
                samples.extend(struct.pack("<hh", sample, sample))
            audio.writeframes(samples)
        if avi_destination and raw_destination:
            build_avi(avi_destination, raw_destination.read_bytes(), bytes(samples))


if __name__ == "__main__":
    main()
