# GTK text view paint probe

A `GtkTextView` with in-process `gtk_text_buffer_set_text("BxGlyphs")`.
`GetImage` on the mapped window tree must see letter ink, not just an
empty light editor. This is the text widget path without a desktop
session. Does not start a window manager.

```bash
ANDROID_SERIAL=<serial> examples/gtk-textview-probe/install-and-run.sh
```
