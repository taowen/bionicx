# WPS baseline after the first desktop-extension matrix

## Result

After Render, XFixes, RandR resource enumeration, XI2 master enumeration and a
minimal XKB map reached 5/5, the existing genuine WPS Writer AArch64 payload was
launched again with diagnostics disabled. It reached the main Writer interface,
rendered the document navigation UI and completed its system-check dialog. The
screen is retained in `evidence/wps-after-desktop-extensions.png`.

This is useful progress but not final acceptance: no document editing,
save/reopen, clipboard, export, Sheets or Presentation workflow was exercised.

## Next gaps selected by the real application

The untraced log identifies three narrow follow-up targets:

```text
failed to get the primary output of the screen
qt.qpa.xcb: failed to select notify events from XKB
qt.qpa.xcb: failed to get core keyboard device info
CANNOT LINK EXECUTABLE "sh": ".../rootfs/usr/lib/libc.so" has bad ELF magic
```

- RandR needs `GetOutputPrimary` (and a controlled output-info test).
- XKB needs event selection and the device-info requests Qt uses.
- The WPS compatibility `popen` path correctly chooses `/system/bin/sh` and
  removes `LD_PRELOAD`, but it still passes the glibc `LD_LIBRARY_PATH` into a
  Bionic process. Android's linker then finds the glibc linker script `libc.so`
  before system libc and rejects its non-ELF contents.

The complete log is retained in
`evidence/wps-after-desktop-extensions.log`. These warnings did not prevent the
main UI from rendering, but they must be removed before WPS is called perfect.
