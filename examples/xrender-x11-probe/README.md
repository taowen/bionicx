# Render probe

libXrender only. One client fills ARGB32/A8 pictures and reads pixels
back for Clear/Src/Over/In/OutReverse/Add/Saturate, rectangle and 1-bit
pixmap clips, nearest filter, solid/gradient sources, and creation-time
repeat. Does not start a desktop daemon.

```sh
ANDROID_SERIAL=<serial> examples/xrender-x11-probe/install-and-run.sh
```
