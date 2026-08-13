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

Untraced `keepassxc` exits 139 about 200 ms after
`Unable to load auto-type plugin: Unknown error`. The plugin
(`libkeepassxc-autotype-xcb.so`) needs `libXtst.so.6`. Gladio
`LD_LIBRARY_PATH` and `QT_XCB_GL_INTEGRATION=none` do not stop the SIGSEGV.
`qt-glx-fbconfig-probe` `qt-keepassxc-spec` is 6/6; this crash is after
choose, same class as Krita after makeCurrent.
