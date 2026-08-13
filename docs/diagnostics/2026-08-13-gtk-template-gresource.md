# GTK composite templates and `.gresource` after ELF fixup

Debian `mousepad` aborted with SIGSEGV after:

```text
Unable to load resource for composite template for type 'GtkStatusbar':
The resource at “/org/gtk/libgtk/ui/gtkstatusbar.ui” does not exist
```

`gtkstatusbar.ui` is compiled into `libgtk-3.so` as `.gresource.gtk`.
`patchelf --set-rpath` (used by `rootfs-elf-fixup.sh`) moved that section
from VMA `0x280` into a rewritten `PT_LOAD` next to `.dynstr`. GTK then
registered a broken blob and template lookup failed.

A second gap: interposed `dlopen("libgtk-3.so.0")` uses the preload
object as the caller, so glibc ignores the executable RUNPATH. The empty
139-byte `ld.so.cache` did not list multiarch directories because
static-PIE `ldconfig` honored `include /etc/ld.so.conf.d` against
Android `/etc`.

## Fixes

- `dlopen` of a bare SONAME searches `$BIONICX_ROOTFS/{usr/,}lib[/aarch64-linux-gnu]`.
- `bxapt-ldconfig.sh` writes `etc/ld.so.conf.bionicx` with app-private
  multiarch paths and points `ldconfig -f` at that file.
- ELF fixup leaves RUNPATH unchanged on objects that contain `.gresource`.
- Stock `libgtk-3.so.0.2417.32` and `libgtksourceview-4.so.0.0.0` were
  restored on the identity-fixed seed after the cache was regenerated.

## x300

```text
BXTEST PASS gtk-dlopen libgtk-3.so.0
BXTEST PASS gio-dlopen libgio-2.0.so.0
BXTEST PASS gtk-init DISPLAY connected
BXTEST PASS gtk-statusbar-ui bytes=1039
BXTEST PASS gtk-dialog-ui bytes=1260
BXTEST PASS gtk-statusbar GtkStatusbar mapped
BXSUMMARY gtk-template passed=6 failed=0
```

Untraced `mousepad` opens the packaged note, paints the menu and
GtkStatusbar, and stays alive. Seed `ed998c09…` was not replaced.

Host: `tests/test-runtime-contract.sh`, `tests/test-bxapt-ldconfig-wrapper.sh`,
`tests/test-rootfs-elf-fixup.sh`, `tests/test-gtk-template-probe.sh`.
