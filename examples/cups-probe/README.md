# CUPS API probe

This probe calls the real glibc CUPS API the same way WPS does:
`dlopen("libcups.so.2")` (QLibrary("cups", 2)), resolve the dest/print
entry points, `cupsGetDests()` for `bionicx-test`, then `cupsPrintFile()`
into the controlled `file:` backend. It is run through the shared
`bionicx-exec` loader with `CUPS_SERVER` pointing at the app-private socket.
