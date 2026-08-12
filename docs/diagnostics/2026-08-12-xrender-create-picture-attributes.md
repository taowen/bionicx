# XRender attributes at picture creation

## Symptom

The real GTK3 file chooser mapped successfully and its Fontconfig/Pango lookup
resolved `Roboto 14`, but text and large Cairo-painted regions were missing.
Cairo creates one-pixel source pictures with `CPRepeat` already set in
`RenderCreatePicture`. BionicX consumed the value list without applying it, so
sampling outside source coordinate `(0, 0)` returned transparent pixels.

## Correction and controlled proof

`CreatePicture` and `ChangePicture` now share the same attribute parser. It
validates and retains repeat mode, clip origin, clip-mask reset, and component
alpha instead of treating the creation values as padding.

`x11-desktop-probe` uses the genuine libXrender API to create a 1x1 red picture
with `CPRepeat=RepeatNormal`, composites it into an 8x8 destination, and reads a
pixel far from the source origin. On x300 `01408BH601027129` it reported:

```text
BXTEST PASS xrender ... create-repeat=0xff0000 ...
BXSUMMARY desktop-x11 passed=8 failed=0 xerrors=0
```

This check specifically fails if attributes are applied only by a later
`ChangePicture` request.
