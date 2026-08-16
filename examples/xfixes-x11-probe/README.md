# XFixes cursor and save-set probe

libXfixes only. One two-connection client covers XFixes 4 cursor
requests, `ChangeSaveSet`, region combine, and region transform
(`Set`/`Translate`/`Invert`/`Extents`/`CreateRegionFromWindow`),
including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/xfixes-x11-probe/install-and-run.sh
```
