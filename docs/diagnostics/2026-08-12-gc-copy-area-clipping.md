# Core GC clipping for CopyArea

## Symptom

After XRender repeat was fixed, GTK text appeared but rectangular portions of
the file chooser were black. Cairo paints a temporary pixmap for each damage
region, installs that region with core `SetClipRectangles`, and copies it to the
window. BionicX skipped opcode 59 and copied the entire temporary pixmap,
including its untouched black pixels.

## Correction and controlled proof

Graphics contexts now retain clip origins and rectangle lists.
`SetClipRectangles` parses the complete list, `ChangeGC` handles the matching
origins and a `None` clip mask, and `CopyArea` intersects its destination with
each GC rectangle while translating the source origin by the same offset.

The core probe seeds an 80x60 destination red and source blue, clips the copy to
20x20, then reads one pixel on each side of the clip boundary. The unrooted
device result was:

```text
BXTEST PASS copy-area-clip inside=0x2060c0 outside=0xa02020
BXSUMMARY passed=17 failed=0 observational_input=yes
```

The GTK damage-copy pattern no longer overwrites the whole window with the
unpainted part of an offscreen buffer.
