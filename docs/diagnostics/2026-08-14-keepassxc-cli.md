# KeePassXC CLI database and GUI crash

## Controlled client

`examples/keepassxc-cli-probe` drives Debian `keepassxc-cli` on the shared
seed: key-file v2.0 `db-create`, `add login`, `ls`, `show` Title/UserName/URL,
reopen, and a non-empty `.kdbx`. Device result:

```text
BXSUMMARY keepassxc-cli passed=6 failed=0
```

The GUI profile opens that same file with `--keyfile` **after** the
MainWindow is mapped (`keepassxc-deferred-open` + D-Bus `openDatabase`).

## Command-line `--keyfile` still 139

`keepassxc --keyfile … file.kdbx` constructs `DatabaseWidget` in
`main()` and only then `bringToFront()`. That first `show()` of the
already-unlocked tree dies in `QWidgetPrivate::showChildren` (NULL
`d_ptr`). Welcome, the unlock form, and unlocking a mapped window do
not. The profile therefore maps first and opens over D-Bus.

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

## Diagnosed 139

`bionicx-exec --diagnose-signals` on the same X server:

```text
signal=11 code=1 address=0x20 x0=0x0
pc libQt5Widgets.so.5.15.15+0x1cf15c
lr libQt5Widgets.so.5.15.15+0x1cf248
```

That is `QWidgetPrivate::showChildren(bool)` loading `child->d_ptr+0x20`
when `d_ptr` is NULL. A generic `QMainWindow`+`QLabel` `show()` on the
same xcb path (`examples/qt-widgets-show-probe`, no Gladio,
`QT_XCB_GL_INTEGRATION=none`) does **not** crash. The bad child is
KeePassXC-specific, not every Qt `show()`.
