# KeePassXC app fixture

Seeds a key-file-only `.kdbx` with Debian `keepassxc-cli` and installs
`keepassxc-deferred-open` for the GUI profile. This is an app helper,
not a protocol probe. The DatabaseWidget NULL `d_ptr` repro is
`keepassxc-db-widget-probe`.

```sh
ANDROID_SERIAL=<serial> examples/keepassxc/seed-db.sh
```
