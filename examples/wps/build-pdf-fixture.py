#!/usr/bin/env python3
"""Build the deterministic two-page PDF used by the WPS integration test."""

from pathlib import Path
import sys


def stream_object(contents: bytes) -> bytes:
    return (
        f"<< /Length {len(contents)} >>\nstream\n".encode()
        + contents
        + b"endstream"
    )


def build_pdf() -> bytes:
    page_one = (
        b"BT\n/F1 30 Tf\n72 690 Td\n(BionicX PDF Page 1) Tj\n"
        b"0 -48 Td\n/F1 18 Tf\n(glibc + X11 on Android) Tj\nET\n"
    )
    page_two = (
        b"BT\n/F1 30 Tf\n72 690 Td\n(BionicX PDF Page 2) Tj\n"
        b"0 -48 Td\n/F1 18 Tf\n(Navigation verified) Tj\nET\n"
    )
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R 6 0 R] /Count 2 >>",
        (
            b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
            b"/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>"
        ),
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        stream_object(page_one),
        (
            b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
            b"/Resources << /Font << /F1 4 0 R >> >> /Contents 7 0 R >>"
        ),
        stream_object(page_two),
        (
            b"<< /Title (BionicX PDF Integration) /Author (BionicX) "
            b"/Creator (examples/wps/build-pdf-fixture.py) >>"
        ),
    ]

    output = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for number, body in enumerate(objects, 1):
        offsets.append(len(output))
        output.extend(f"{number} 0 obj\n".encode())
        output.extend(body)
        output.extend(b"\nendobj\n")

    xref_offset = len(output)
    output.extend(f"xref\n0 {len(objects) + 1}\n".encode())
    output.extend(b"0000000000 65535 f \n")
    for offset in offsets[1:]:
        output.extend(f"{offset:010d} 00000 n \n".encode())
    output.extend(
        (
            f"trailer\n<< /Size {len(objects) + 1} /Root 1 0 R "
            f"/Info 8 0 R >>\nstartxref\n{xref_offset}\n%%EOF\n"
        ).encode()
    )
    return bytes(output)


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} OUTPUT.pdf")
    destination = Path(sys.argv[1])
    destination.write_bytes(build_pdf())


if __name__ == "__main__":
    main()
