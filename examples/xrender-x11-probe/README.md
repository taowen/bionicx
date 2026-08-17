# Render probe

libXrender only. One client fills ARGB32/A8 pictures and reads pixels
back for Clear/Src/Over/In/OutReverse/Add/Saturate, rectangle and 1-bit
pixmap clips, nearest filter, solid/gradient sources, creation-time
repeat, a 24-in-32 fill Over a near-transparent title tile so unused
destination alpha stays opaque, and a window Picture that still paints
after the window grows from 1x1, CompositeTrapezoids fills a convex
trap, and CompositeGlyphs32 paints a 32-bit glyph id. Advertises
Render 0.4 so cairo will send those traps. Does not start a desktop
daemon.

```sh
ANDROID_SERIAL=<serial> examples/xrender-x11-probe/install-and-run.sh
```
