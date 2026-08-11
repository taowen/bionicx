# XFixes 2.0 Region lifecycle

## Controlled requirement

`x11-desktop-probe` links the real AArch64 glibc `libXfixes` and requires more
than extension discovery. It creates a Region from one wire rectangle, fetches
the Region back from the server, checks all four rectangle fields, and destroys
the resource.

## Server implementation

`XFixesExtension` provides the stateful subset needed for that transaction:

- `QueryVersion` negotiates XFixes 2.0, the first version containing Regions;
- `CreateRegion` decodes the request-length-bounded rectangle list;
- `FetchRegion` returns the computed extents and complete rectangle payload;
- `DestroyRegion` releases the ID;
- a client disconnect releases any Region IDs it did not explicitly destroy.

The implementation reserves distinct event/error bases and reports BadRegion
for an unknown Region. Other XFixes 1.0 and 2.0 requests, including selection
notifications and clip-region attachment, remain intentionally unsupported and
must gain their own controlled tests before use by Chrome or WPS is claimed.

## x300 result

The APK and freshly regenerated bundle ran without tracing on device
`01408BH601027129`:

```text
BXTEST PASS xrender      version=0.1 event=0 error=0
BXTEST PASS xfixes       version=2.0 rectangles=1
BXSUMMARY desktop-x11 passed=2 failed=3 xerrors=0
```

The raw launch and result stream is retained in
`evidence/x11-desktop-probe-xfixes.log`. XFixes Region state itself has no
visual representation, so the protocol/result log is the decisive evidence.
