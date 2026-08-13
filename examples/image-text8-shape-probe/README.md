# ImageText8 and SHAPE probe

Genuine AArch64 glibc/libX11 client for the two xterm gaps: core
`XDrawImageString` (opcode 76) must fill the GC background and paint
foreground glyphs, and `QueryExtension(SHAPE)` must return a working 1.1
implementation that stores `ShapeRectangles`.

The payload is installed against the existing seed rootfs. It does not
replace `/files/rootfs`.

```sh
ANDROID_SERIAL=<serial> examples/image-text8-shape-probe/install-and-run.sh
```
