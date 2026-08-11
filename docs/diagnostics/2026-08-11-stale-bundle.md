# Stale integration bundle false regression

While checking the XRender change, the core X11 probe remained 13/13 but the
default runtime probe unexpectedly timed out in robust mutex owner death. The
two local bundle libc hashes identified the problem:

```text
54a06275...  build/runtime-probe-bundle/rootfs/usr/lib/libc.so.6
bf6f6b18...  build/runtime-probe-repro-bundle/rootfs/usr/lib/libc.so.6
```

The install scripts rebuilt only when their output directory did not exist, so
the default directory survived the robust-mutex runtime correction. Explicitly
rebuilding replaced it with libc SHA-256
`bf6f6b184710068d1766d95b46d8fa3c578ef2a4da4a0d932f9a8c92bc97c4ee` and
restored `BXSUMMARY runtime passed=20 failed=0`.

All controlled-app install scripts now rebuild their bundle layer before
installation. Expensive glibc compilation remains content-addressed, while the
cheap client compilation, dependency copy, and prefix relocation cannot become
stale. This trades a few seconds per run for trustworthy diagnostic results.
