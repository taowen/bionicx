# Cross-client X11 server-grab probe

This genuine AArch64 glibc/libX11 client verifies that `GrabServer` freezes
round-trip requests from another connection while the owner continues making
progress. It then checks both explicit `UngrabServer` and grab-owner disconnect
release deferred requests. A connection opened during the grab must also block
its setup handshake and become usable after release.

```sh
ANDROID_SERIAL=<serial> examples/server-grab-x11-probe/install-and-run.sh
```

Success requires five strict ordering checks and a normal process exit.
