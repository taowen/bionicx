#!/usr/bin/env python3
"""Build a deterministic web-seeded torrent for qBittorrent integration."""

import hashlib
import pathlib
import sys


PIECE_LENGTH = 64 * 1024
PAYLOAD_LENGTH = 256 * 1024


def bencode(value):
    if isinstance(value, int):
        return b"i" + str(value).encode("ascii") + b"e"
    if isinstance(value, bytes):
        return str(len(value)).encode("ascii") + b":" + value
    if isinstance(value, list):
        return b"l" + b"".join(bencode(item) for item in value) + b"e"
    if isinstance(value, dict):
        return b"d" + b"".join(
            bencode(key) + bencode(value[key]) for key in sorted(value)
        ) + b"e"
    raise TypeError(type(value))


def main():
    payload_path = pathlib.Path(sys.argv[1])
    torrent_path = pathlib.Path(sys.argv[2])
    payload = bytes((index * 29 + index // 251) & 0xFF
                    for index in range(PAYLOAD_LENGTH))
    pieces = b"".join(
        hashlib.sha1(payload[offset:offset + PIECE_LENGTH]).digest()
        for offset in range(0, len(payload), PIECE_LENGTH)
    )
    torrent = {
        b"comment": b"BionicX deterministic localhost web-seed probe",
        b"created by": b"BionicX",
        b"info": {
            b"length": len(payload),
            b"name": payload_path.name.encode("utf-8"),
            b"piece length": PIECE_LENGTH,
            b"pieces": pieces,
        },
        b"url-list": b"http://127.0.0.1:18765/" + payload_path.name.encode("utf-8"),
    }
    payload_path.parent.mkdir(parents=True, exist_ok=True)
    payload_path.write_bytes(payload)
    torrent_path.write_bytes(bencode(torrent))


if __name__ == "__main__":
    main()
