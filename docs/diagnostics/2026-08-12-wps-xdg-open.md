# Opening a Writer-exported PDF

## Decisive syscall boundary

Writer exported a structurally valid PDF but its completion dialog reported an
open error. Opt-in libc command wrappers did not observe a launch. A root-only
diagnostic `strace` (never part of runtime) showed Qt checking every inherited
Android `PATH` directory for `xdg-open`, then `gnome-open`, Chrome, Firefox,
Mozilla, and Opera. Every `newfstatat` returned `ENOENT`; Qt therefore stopped
before creating a child.

The compact excerpt is retained in `evidence/wps-xdg-open.log`. This also
confirmed that the earlier empty `bash` desktop-association check was not the
document opener.

## Failed shell boundary

An app-private `xdg-open` shell script was discoverable, but its
`#!/system/bin/sh` interpreter could not start. The detached child inherits the
parent glibc `LD_LIBRARY_PATH`, so Android's linker found the GNU `libc.so`
linker script first and rejected it as a Bionic ELF. Clearing the environment
inside the script is too late because the shebang interpreter has already
failed.

## glibc desktop dispatcher

`native/desktop/bionicx-open.c` is a small genuine AArch64 glibc executable.
It accepts a path or `file://` URI, strictly decodes percent escapes, chooses a
handler by suffix, and replaces itself with the configured executable. The WPS
profile puts its app-private `bin` first in `PATH` and maps PDF files to its
already relocated `wpspdf` entrypoint. No desktop MIME database, D-Bus daemon,
shell, root service, or protocol bypass is involved.

The unrooted Writer workflow now exports `PS on Android.pdf`, presses **Open
File**, starts `wpspdf` as app UID 10194, and renders the exact exported page in
the same X server. The process argument contains the decoded space-bearing
path, and the PDF remains a one-page WPS Writer PDF 1.7 with all expected text.
The clean viewer result is
`evidence/wps-writer-open-exported-pdf.png`.

A controlled device invocation separately passes
`file:///tmp/BionicX%20Probe.txt` and requires the unsupported-handler result to
contain `/tmp/BionicX Probe.txt`. This exercises URI decoding without launching
an application or relying on the proprietary workflow.
