# ImageText8 and SHAPE for package xterm

Debian `xterm` without `-fa` draws with core `XDrawImageString`
(opcode 76). The embedded server handled PolyText8 but returned
`BadImplementation` for ImageText8. IceWM and xterm also logged
`Xlib: extension "SHAPE" missing`.

## Controlled client

`examples/image-text8-shape-probe` is a genuine AArch64 glibc/libX11
client installed against the existing seed (`rootfs_payload=none`).
It paints a known field, calls `XDrawImageString`, and reads the
pixels back. Background fill and foreground glyphs must both be
present. It then queries SHAPE 1.1, applies `ShapeRectangles`,
checks extents/GetRectangles, and selects ShapeNotify.

## Server

ImageText8/16 fill the GC background rectangle using the virtual
fixed-face metrics (8x14) and paint Latin-1 glyphs. SHAPE is
advertised; Rectangles/Combine/Offset/QueryExtents/GetRectangles
and SelectInput are implemented. Bounding/clip kinds are stored
for protocol replies. The compositor still draws rectangular
windows. ShapeInput continues to clip pointer hit testing.

## x300

```text
BXTEST PASS display-connect :0
BXTEST PASS image-text8 field=9984 background=1749 foreground=267
BXTEST PASS shape-query event=65 error=0
BXTEST PASS shape-version 1.1
BXTEST PASS shape-rectangles shaped=1 count=1 rect=120x40+12+16 extents=120x40+12+16
BXTEST PASS shape-select-input mask=0x1
BXSUMMARY image-text8-shape passed=6 failed=0
```

Seed `ed998c09…` was not replaced. Host
`tests/test-image-text8-shape-probe.sh` checks the profile and
opcodes.
