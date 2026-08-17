# MIT-SHM CreatePixmap + PresentPixmap probe

Chrome Ozone software present fills a SysV SHM segment, wraps it with
`XShmCreatePixmap`, then `PresentPixmap`s that pixmap onto the window.
This client is that path without Chromium: `shmget` / `XShmAttach` /
`XShmCreatePixmap` / Present. A missing pixmap ID leaves the window on
its background color (the document paint hole).

```sh
ANDROID_SERIAL=<serial> examples/mit-shm-present-x11-probe/install-and-run.sh
```
