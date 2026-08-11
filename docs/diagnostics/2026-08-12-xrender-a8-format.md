# XRender A8 glyph-format discovery

## Observed boundary

The first real Xft client successfully registered and matched app-private
Liberation fonts through Fontconfig, but both `XftFontOpenName` calls returned
null. A direct `XRenderFindStandardFormat(PictStandardA8)` check isolated the
cause: BionicX advertised only ARGB32 and A1, while Xft requires an 8-bit alpha
format for anti-aliased glyph masks.

## Protocol correction

`QueryPictFormats` now publishes a direct depth-8 format with an `0xff` alpha
mask. It is deliberately present in the global format list but absent from the
screen's core drawable depths: glyph sets can use A8, while BionicX does not
falsely advertise depth-8 core Pixmaps.

The strict `x11-desktop-probe` now requires this exact standard format in
addition to its ARGB32 picture fill and pixel readback. On x300 all eight
desktop checks passed with zero X errors:

```text
BXTEST PASS xrender version=0.1 event=0 error=0 a8=1 alpha-mask=0xff
BXSUMMARY desktop-x11 passed=8 failed=0 xerrors=0
```

The full result is retained in `evidence/x11-desktop-probe-xrender-a8.log`.
With A8 available, Xft opens distinct Regular and Bold faces and proceeds to
Render `CreateGlyphSet`, `AddGlyphs`, and `CompositeGlyphs8`; those operations
are the next controlled server boundary rather than being claimed here.
