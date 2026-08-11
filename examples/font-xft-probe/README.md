# Fontconfig and Xft integration probe

This is a genuine AArch64 glibc client linked to Fontconfig, FreeType, Xft,
XRender, and Xlib. It registers two app-private Liberation Sans files, requires
Fontconfig to resolve distinct Regular and Bold faces, and renders four UTF-8
lines through Xft. The repository does not store the fonts; the cached Debian
cross-builder supplies the open font files when constructing the bundle.

Run it on Android without root, PRoot, Termux, or Frida:

```sh
ANDROID_SERIAL=<serial> examples/font-xft-probe/install-and-run.sh
```

A passing process prints `BXTEST PASS font-xft checks=4/4` only after an
`XSync` reports no X protocol error. A screenshot remains useful evidence that
the glyph masks and regular/bold distinction are visually correct.
