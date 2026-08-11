# glibc DNS/network/X11 probe

This genuine AArch64 glibc program makes the Android-to-glibc resolver bridge
observable without Chromium. It verifies the injected glibc `res_state`, sends
an A query through `libresolv`, connects to the result, validates the reserved
Example Domain HTTP response, and renders the final result through real libX11.

```sh
ANDROID_SERIAL=<serial> examples/network-x11-probe/install-and-run.sh
adb -s <serial> logcat -d -s BionicX | grep BXTEST
```

The live network checks intentionally use `example.com`, the IANA-reserved
documentation domain. A disconnected device is expected to fail at the
`android-dns-config` or `dns-a-query` boundary rather than being skipped.
