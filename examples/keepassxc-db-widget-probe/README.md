# KeePassXC DatabaseWidget show() probe

Empty KeePassXC (welcome) stays up. Opening a `.kdbx` with `--keyfile`
before `bringToFront()` dies in `QWidgetPrivate::showChildren` with a
NULL `d_ptr` (`libQt5Widgets.so.5.15.15+0x1cf15c`, fault `0x20`). This
client builds the same hidden `QStackedWidget` + nested `QSplitter` +
`QTreeView` + edit-page tree `DatabaseWidget` constructs, then `show()`s
it. It does not launch `keepassxc`.

```sh
ANDROID_SERIAL=<serial> examples/keepassxc-db-widget-probe/install-and-run.sh
```

Expect `BXSUMMARY keepassxc-db-widget passed=4 failed=0` (welcome
`show()`, DatabaseWidget-like tree `show()`, current page visible, `exec`).
