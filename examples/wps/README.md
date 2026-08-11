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

## Writer, Spreadsheets, and Presentation entrypoints

The Debian package's `et` and `wpp` entrypoints retain `/lib/ld-linux...`, while
direct mode needs the app-private loader and Spreadsheets additionally loads
`libXtst.so.6`. Prepare all three already-installed entrypoints and install the
audited ARM64 Xtst runtime dependency without root:

```sh
ANDROID_SERIAL=01408BH601027129 examples/wps/install-entrypoints.sh
```

Then select `profiles/wps-office.json` for Writer,
`profiles/wps-spreadsheets.json` for Spreadsheets, or
`profiles/wps-presentation.json` for Presentation. All profiles deliberately
share the `wps-office` application ID, app tree, HOME, and compatibility module;
only the selected entry ELF and `argv[0]` differ. Proprietary WPS files are read
from and written back to the user's installed Android app-private tree and are
never added to this repository.

The preparation also creates a mode-`0700` literal `office6/??` directory.
WPS 11.1.0.11720's Presentation serializer uses that relative directory in its
PPTX temporary-file template under the profile's `LANG=C`; see the retained
[diagnosis](../../docs/diagnostics/2026-08-12-wps-file-trace.md). This narrow
deployment provision avoids changing `mkstemp` behavior for other clients.

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

Formatting can be asserted against the OOXML run properties as well:

```sh
ANDROID_SERIAL=01408BH601027129 \
  examples/wps/verify-docx.sh BionicX.docx BionicX_WPS_2026 abc_A \
  --bold Bold_WPS_2026
```

Repeated paragraphs can be counted exactly. This is useful for proving that a
clipboard paste changed the saved document instead of merely repainting the
editor:

```sh
ANDROID_SERIAL=01408BH601027129 \
  examples/wps/verify-docx.sh BionicX.docx abc_A \
  --bold Bold_WPS_2026 --count BionicX_WPS_2026 2
```

## Deterministic Microsoft-font substitutes

Android system images do not normally contain Calibri, Cambria, Arial, or
Times New Roman. Install metrically compatible open fonts into the WPS app
tree and configure deterministic aliases without root:

```sh
ANDROID_SERIAL=01408BH601027129 examples/wps/install-open-fonts.sh
```

The installer copies Liberation Sans, Serif, and Mono from the host, installs
`fonts.conf`, verifies all six device files, and invalidates only its dedicated
Fontconfig cache. Restart WPS after running it. The configured font paths point
at `${APP}` directly; `files/wps-root` is legacy migration state and must not
be treated as the application tree.
