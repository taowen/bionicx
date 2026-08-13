# KeePassXC DatabaseWidget show() probe

Empty KeePassXC (welcome) stays up. Opening the fixture `bionicx.kdbx`
dies in `QWidgetPrivate::showChildren` with a NULL `d_ptr`
(`libQt5Widgets.so.5.15.15+0x1cf15c`, fault `0x20`). This client builds
the same hidden `QStackedWidget` + nested `QSplitter` + `QTreeView` +
edit-page tree `DatabaseWidget` constructs, then `show()`s it.

```sh
ANDROID_SERIAL=<serial> examples/keepassxc-db-widget-probe/install-and-run.sh
```

Expect `BXSUMMARY keepassxc-db-widget passed=4 failed=0` (welcome
`show()`, DatabaseWidget-like tree `show()`, current page visible, `exec`).

The same script then launches Debian `keepassxc` through
`keepassxc-deferred-open`: map the MainWindow first, then
`openDatabase` the fixture over D-Bus. Command-line `--keyfile` before
`bringToFront()` is the NULL `d_ptr` path; opening after the window is
mapped shows the `login` / `bionicx` entry. Expect the process to stay
up and `BXSUMMARY keepassxc-db-widget-gui passed=1 failed=0`.
