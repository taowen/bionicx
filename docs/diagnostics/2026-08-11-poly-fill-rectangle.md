# X11 PolyFillRectangle foreground semantics

## Symptom

The core request completed without an X error, but a requested blue Pixmap was
white on screen. The original probe therefore exposed why request-success alone
is not a sufficient graphics-server test.

## Root cause and correction

`DrawRequests.polyFillRectangle` passed `GraphicsContext.getBackground()` to
the drawable. X11 PolyFillRectangle uses the GC foreground for solid fills.
The handler now passes `getForeground()`.

The regression fills a Pixmap with `0x3264c8`, copies it to a Window, and reads
one pixel back from both drawables with `XGetImage`. It masks the unused high
byte of the server's 24-depth/32-bpp representation before comparing the exact
RGB value. The previous white value still fails this assertion.

## x300 regression

```text
BXTEST PASS pixmap-gc-copy pixel=0x3264c8
BXSUMMARY passed=11 failed=1 observational_input=no
```

Visual evidence: [x11-probe-fill.png](../../evidence/x11-probe-fill.png),
SHA-256
`dd3b10e10e8e950d78001ca0ff68091738d71b1d07e0f2b6a96f61bc8fe683b4`.
The remaining red panel represents the separate PolyText8 failure.
