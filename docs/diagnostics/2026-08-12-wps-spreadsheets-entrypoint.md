# WPS Spreadsheets entrypoint and first dependency closure

## Reproduction

Launching the installed AArch64 `et` ELF with the Writer profile's direct-mode
environment exited before glibc startup:

```text
bionicx-exec: execv(.../office6/et): No such file or directory
wps-office exited with 6
```

The file existed and was executable. ELF inspection gave the decisive cause:
Writer had already been relocated to BionicX's app-private loader, but the two
other suite entrypoints had not:

```text
wps  interpreter /data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
et   interpreter /lib/ld-linux-aarch64.so.1
wpp  interpreter /lib/ld-linux-aarch64.so.1
```

After relocating `et`, startup advanced deterministically to the next missing
object reported by its real `libetmain.so` closure:

```text
dlopen .../libetmain.so failed , error: libXtst.so.6: cannot open shared object file
```

## Reproducible correction

`install-entrypoints.sh` reads the user's installed proprietary entrypoints
through Android `run-as`, changes only mismatched `PT_INTERP` values with host
`patchelf`, writes them back through the same unprivileged boundary, and checks
the result. It also obtains ARM64 `libXtst.so.6` from BionicX's
content-addressed Debian builder and verifies the host/device SHA-256. The
builder now declares `libxtst-dev:arm64`, and normal probe rootfs bundles carry
the same library. Its required libc version is only `GLIBC_2.17`, below the
pinned Android-compatible glibc 2.39 runtime.

The x300 preparation reported:

```text
BXELF entry=wps before=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1 after=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
BXELF entry=et before=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1 after=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
BXELF entry=wpp before=/lib/ld-linux-aarch64.so.1 after=/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1
BXELF library=libXtst.so.6 sha256=134cb0235ec9e096c5337f379548c05b7e0db5847b49db340ebdaa095fac24d7
```

With `profiles/wps-spreadsheets.json`, genuine WPS Spreadsheets then reached its
full-screen home UI and completed the same font health check as Writer. There
was no missing ELF, unsupported X request, `BadImplementation`, or fatal signal
during this startup checkpoint. See `evidence/wps-spreadsheets-home.png` and
`evidence/wps-spreadsheets-entrypoint.log`.

## Follow-up resolution

The first New Document attempt terminated with status 139, but the same action
could not reproduce the failure in either short-lived signal-diagnostic mode or
two subsequent normal untraced launches. The diagnostic run reached `Book1`;
the final untraced run created, calculated, saved, and cold-reopened a workbook.
No workaround was added for a single non-reproducible process exit. See
`2026-08-12-wps-spreadsheets-formula.md` for the durable functional check.
