# GTK GDK filter grab probe

A GtkWindow plus `gdk_window_add_filter(NULL)` is the grabber. XTEST
clicks a sync `XIGrabButton` on that window, a second-connection peer,
a redirected peer, and a hovered peer. The filter must see the press
and `AllowEvents`. Isolated GTK reports `core=0 xi=1`: GDK eats real
core `ButtonPress` and the pointer path is the XI2 cookie.

```bash
ANDROID_SERIAL=<serial> examples/gtk-gdk-grab-probe/install-and-run.sh
```
