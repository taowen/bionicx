# KeePassXC CLI database and GUI crash

## Controlled client

`examples/keepassxc-cli-probe` drives Debian `keepassxc-cli` on the shared
seed: key-file v2.0 `db-create`, `add login`, `ls`, `show` Title/UserName/URL,
reopen, and a non-empty `.kdbx`. Device result:

```text
BXSUMMARY keepassxc-cli passed=6 failed=0
```

The GUI profile opens that same file with `--keyfile`.

## Still failing

Untraced `keepassxc` still exits 139 after the controlled path below
passes. The remaining crash is past XTEST, plugin `dlopen`, and the first
`glClear` after makeCurrent.

## Controlled follow-up

`examples/qt-gui-segv-probe` is 7/7 on device:

```text
BXSUMMARY qt-gui-segv passed=7 failed=0
```

Winlator now advertises XTEST 2.2 and maps `XTestFakeKeyEvent` onto the
existing inject helpers. `libXtst.so.6` and
`libkeepassxc-autotype-xcb.so` `dlopen` with `RTLD_NOW`. KeePassXC
`isAvailable()` only needs `XQueryExtension("XTEST")` and
`XQueryExtension("XInputExtension")`.

An unmapped 64×64 window used to make Gladio skip display-buffer creation
(`getWindowSize` 0×0), so the first `glClear` returned
`GL_INVALID_FRAMEBUFFER_OPERATION` (0x506). Mapping the window, plus a
64×64 placeholder when the size is still 0, makes
`qt-draw-after-current` report `OpenGL ES 3.2 Gladio`.

The GUI profile now uses Gladio the same way as Krita. `qglx_findConfig`
no longer fires; the process still dies with status 0xb.
