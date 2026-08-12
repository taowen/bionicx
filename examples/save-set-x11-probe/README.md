# Cross-client X11 save-set probe

This genuine AArch64 glibc/libX11 client creates an application window on one
connection and a manager-owned frame on another. It exercises save-set insert,
delete and reinsert, reparents the app into the frame, then disconnects the
manager. The application window must survive, return to the root at the same
screen coordinates, and remain viewable.

```sh
ANDROID_SERIAL=<serial> examples/save-set-x11-probe/install-and-run.sh
```
