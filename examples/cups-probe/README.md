# CUPS API probe

This probe calls the real glibc `cupsGetDests()` API and requires the
controlled `bionicx-test` destination. It is run through the shared
`bionicx-exec` loader with `CUPS_SERVER` pointing at the app-private socket.
