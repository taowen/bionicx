# XFixes cursor and save-set probe

libXfixes only. One two-connection client covers XFixes 4 cursor
requests, `ChangeSaveSet`, and region combine (`Intersect`/`Union`/`Subtract`),
including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/xfixes-x11-probe/install-and-run.sh
```
