# Cross-client X11 clipboard probe

This real AArch64 glibc/libX11 application opens two independent X11
connections. One owns `CLIPBOARD`; the other requests `UTF8_STRING`. The owner
receives `SelectionRequest`, writes the payload to the other client's window,
and sends `SelectionNotify`. It also verifies replacement `SelectionClear`,
release to `None`, and the no-owner conversion response.

```sh
ANDROID_SERIAL=<serial> examples/clipboard-x11-probe/install-and-run.sh
```

Success requires five strict checks, including owner-disconnect cleanup, the
exact cross-client property bytes, zero X errors, and a normal process exit. No
Android clipboard shortcut or same-client simulation is involved.
