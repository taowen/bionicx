# GTK GDK filter grab probe

A GtkWindow plus `gdk_window_add_filter(NULL)` is the grabber. XTEST
clicks a sync `XIGrabButton` on that window, a second-connection peer,
a redirected peer, and a hovered peer. The filter must see the press
and `AllowEvents`. Isolated GTK used to report `core=0 xi=1`: GDK
eats real core `ButtonPress` and the pointer path is the XI2 cookie.
A passive sync-grab press now also carries the SendEvent bit so a
GDK filter that missed the cookie can still thaw. The detail line
is `core= send= xi= allow=`. `--core-only` is a locating dump of a
core `XGrabButton` grabber with `GDK_CORE_DEVICE_EVENTS=1`.

```bash
ANDROID_SERIAL=<serial> examples/gtk-gdk-grab-probe/install-and-run.sh
```
