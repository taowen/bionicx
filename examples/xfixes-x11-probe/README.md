# XFixes cursor and save-set probe

libXfixes only. One two-connection client covers XFixes 4 cursor
requests (`SelectCursorInput`, `GetCursorImageAndName`, `Set`/`GetCursorName`,
`ChangeCursor`/`ChangeCursorByName`, `Hide`/`ShowCursor`) plus
`ChangeSaveSet`, including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/xfixes-x11-probe/install-and-run.sh
```
