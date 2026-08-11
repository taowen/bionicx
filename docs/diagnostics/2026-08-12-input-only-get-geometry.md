# InputOnly window geometry and cross-process window observer

## Chrome trigger

Chrome's full-screen X11 window contains an unmapped override-redirect child
used for clipboard input. A separate genuine AArch64 glibc/Xlib client found
the child through `QueryTree`, but `XGetWindowAttributes` terminated with:

```text
BXWINDOW_ERROR code=9 request=14 minor=0 resource=0x400000 text=BadDrawable
```

Request 14 is core `GetGeometry`. BionicX stored `InputOnly` windows in the
window manager without a backing `Drawable`, while the handler looked only in
the drawable manager. This made `GetWindowAttributes` internally inconsistent:
its attribute reply succeeded, but Xlib's following geometry request failed.
The [X11 protocol specification](https://www.x.org/releases/X11R7.7/doc/xproto/x11protocol.pdf)
explicitly says that passing an `InputOnly` window to `GetGeometry` is legal.

## Controlled regression

`x11-probe` now creates a 40 by 30 `InputOnly` child at `(3,4)`, then requires
both `XGetWindowAttributes` and `XGetGeometry` to succeed. The server obtains
window geometry from the window object and returns depth zero without creating
a fake drawable. On x300 `01408BH601027129` the complete core suite reports:

```text
BXTEST PASS input-only-geometry class=2 geometry=40x30+3+4 border=0 depth=0
BXSUMMARY passed=15 failed=0 observational_input=yes
```

The raw run is retained in `evidence/x11-input-only-geometry.log`.

## Cross-process application proof

`examples/x11-window-tree` is a standalone glibc/Xlib observer. It recursively
enumerates another client's windows and prints title, class, map state,
override-redirect state, and geometry. It runs through the ordinary untraced
BionicX executor while the observed application remains live.

Against Chrome after the fix it reports the top-level browser window, its
utility window, and the clipboard `InputOnly` child with zero X errors:

```text
BXWINDOW depth=0 id=0x400000 map=unmapped override=1 geometry=10x10-100-100 title=Chromium clipboard class=-/-
BXSUMMARY window-tree windows=3 xerrors=0
```

See `evidence/chrome-window-tree-input-only.log`. Invoking Chrome's **Save page
as...** still creates no additional X window, so the file chooser remains a
separate desktop-service diagnosis rather than being falsely attributed to
this corrected core request.
