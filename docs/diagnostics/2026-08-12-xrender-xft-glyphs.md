# Real Xft glyph upload and composition

## Controlled client

`font-xft-probe` is a genuine AArch64 glibc application linked against real
Fontconfig, FreeType, Xft, XRender, and Xlib. Its bundle contains two open
Liberation Sans files supplied by the pinned Debian cross-builder. At runtime
it registers that app-private directory, requires distinct Regular and Bold
matches, opens both faces through Xft, and renders four UTF-8 lines.

This reproduces the WPS bold-font boundary without WPS state, dialogs, or
proprietary payloads. The ELF dependency-closure check also caught and fixed a
missing `libbz2.so.1.0` before device installation.

## Protocol sequence and implementation

Once A8 discovery was available, the real client emitted a deterministic
Render sequence: minor 17 `CreateGlyphSet`, repeated minor 20 `AddGlyphs`, and
minor 23 `CompositeGlyphs8`. BionicX now implements:

- client-owned A8/A1 glyph sets, references, freeing, and disconnect cleanup;
- glyph metrics and image decoding, including per-row 32-bit padding;
- bounded mask allocation and glyph deletion;
- `CompositeGlyphs8` element, glyph-set switch, delta, and pen advancement;
- 8-bit mask alpha-over blending into the destination Drawable.

The implementation deliberately accepts only `PictOpOver` for glyph
composition and does not claim CompositeGlyphs16/32, arbitrary Render
operators, or component-alpha masks.

## Device result

On x300 the untraced app rendered regular and bold text in separate colors,
synchronized with zero X errors, remained visible for eight seconds, released
its resources, and exited zero:

```text
BXFONT regular file=.../LiberationSans-Regular.ttf style=Regular
BXFONT bold file=.../LiberationSans-Bold.ttf style=Bold
BXTEST PASS font-xft checks=4/4 display=:0
font-xft-probe exited with 0
```

`evidence/font-xft-probe.png` is the rendered proof and
`evidence/font-xft-probe.log` is the process log. The existing desktop suite
was then rerun unchanged and passed 8/8 with zero X errors, retained in
`evidence/x11-desktop-probe-after-xft-glyphs.log`.
