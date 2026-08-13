# Qt Widgets show() probe

KeePassXC dies in `QWidgetPrivate::showChildren` with a NULL `d_ptr`
(`libQt5Widgets.so.5.15.15+0x1cf15c`, fault `0x20`). This is a minimal
`QMainWindow` + `QLabel` `show()` on the same Gladio/Xcb path.

```sh
ANDROID_SERIAL=<serial> examples/qt-widgets-show-probe/install-and-run.sh
```

Expect `BXSUMMARY qt-widgets-show passed=3 failed=0` (simple `QLabel`,
KeePassXC-like menu/toolbar/tree/tray chrome, then `exec`).
