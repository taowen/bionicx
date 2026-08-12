# Cairo XRender compositing path

## Real-client boundary

A genuine AArch64 glibc GTK3 client constructed and mapped a
`GtkFileChooserDialog`, but GDK terminated after receiving `BadValue` from
Render `Composite`. Logging every protocol error with sequence, major/minor,
error code and offending value made the failure deterministic:

```text
request error seq=369 major=152 minor=8 code=2 data=12
request error seq=372 major=152 minor=26 code=2 data=5
```

The values are Cairo's standard `PictOpAdd` (12) and `PictOpIn` (5), not bad
Picture IDs. After those were implemented, the next clean run isolated
`PictOpOutReverse` (8). This avoided debugger or Frida attachment and did not
change the client's behavior.

## Protocol and pixel correction

BionicX now supports the Render requests Cairo used in this path:

- depth-8 Pixmaps and A8 Pictures, including `PutImage` storage;
- `ChangePicture`, picture clip rectangles and accepted standard filters;
- solid-fill and linear-gradient source Pictures;
- `Composite` with Src, Over, OutReverse and Add;
- `FillRectangles` with Clear, Src, Over and In;
- destination clip intersection and A8 mask alpha.

The implementation applies real Porter-Duff alpha arithmetic. The controlled
libXrender probe first computes `0x80` using In followed by Add, then requires
OutReverse with a `0x40` source to produce `0x60`. Its full x300 run remains
8/8 with zero X errors in `evidence/x11-desktop-probe-cairo-render.log`.

## GTK result and remaining visual gap

The ordinary untraced app process then completed a real GTK3 lifecycle:

```text
BXTEST PASS gtk-file-chooser GtkFileChooserDialog
BXSUMMARY gtk3 passed=8/8 failed=0
chrome exited with 0
```

The process evidence is `evidence/gtk3-file-chooser-render.log`. The screenshot
`evidence/gtk3-file-chooser-render.png` deliberately records that this is not
yet a visual-completeness claim: the dialog has large black regions and
missing text despite completing without a Render error. That defect remains a
separate glyph/window-composition diagnostic boundary.

The remaining XI2 minor 40/59 and XKB minor 21 probes return
`BadImplementation`; GTK handles those optional capability probes and they do
not cause this run to fail.
