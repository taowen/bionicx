# WPS Office: first BionicX application profile

This profile runs the genuine AArch64 Linux WPS Writer ELF. BionicX does not
ship WPS, its fonts, or a Linux root filesystem. Obtain WPS from its publisher
and make sure your redistribution/use complies with its license.

## Expected trees

Install the application tree below `${APP}` and the glibc runtime below
`${RUNTIME}`. On Android those tokens resolve to:

```text
/data/data/io.taowen.bx/files/apps/wps-office
/data/data/io.taowen.bx/files/rootfs
```

The profile expects these important files:

```text
${APP}/opt/kingsoft/wps-office/office6/wps
${APP}/etc/fonts/fonts.conf
${RUNTIME}/usr/lib/ld-linux-aarch64.so.1
${RUNTIME}/usr/lib/libc.so.6
${RUNTIME}/usr/share/X11/locale/locale.dir
```

WPS is the first example that needs a compatibility module. `wps-compat`
implements the small, process-local SysV semaphore subset used by WPS because
Android's app seccomp policy blocks the AArch64 SysV IPC syscalls. It also
routes glibc `popen()` through `/system/bin/sh`. This module is selected by the
profile and is not loaded for other applications.

WPS currently uses `direct` mode to preserve `/proc/self/exe`. Its `PT_INTERP`
must therefore point at BionicX's app-private loader. When migrating the
original POC, the old and new Android package prefixes have equal byte length,
so `tools/relocate-prefix.py` can safely relocate both the interpreter and the
Winlator-patched libxcb socket prefix.

The root-only `migrate-poc-device.sh` is a development convenience for a device
that already has the POC installed as `com.winlator`. Root is used only to copy
that existing private installation; the resulting WPS runtime is launched by
the normal BionicX app UID without root, PRoot, Termux, or Frida.

## Writer save/reopen integration check

After saving a document from Writer to its default `Documents` directory,
validate the actual app-private OOXML file from the host:

```sh
ANDROID_SERIAL=01408BH601027129 \
  examples/wps/verify-docx.sh BionicX.docx BionicX_WPS_2026 abc_A
```

The check reads through Android's `run-as` boundary, verifies every ZIP member,
requires the OOXML package roots, parses `word/document.xml`, and matches whole
paragraphs. It does not need root and does not mistake a rendered screenshot or
a merely existing file for a successful Writer save.
