# WPS in the shared rootfs and direct ELF dependencies

## Package transaction

Device `01408BH601027129` installed WPS Office 11.1.0.11720 into the same
Debian 13 rootfs and dpkg database already used by IceWM and xterm. The
external deb matched SHA-256
`172d8bcf3e3bc756994ae5adf66f63f22691e1ab0d18986c50bb6b6ab7f62948`.
The final `dpkg --audit` output was empty, and `wps-office`, `xdg-utils`, and
`libglu1-mesa` all had status `ii`.

WPS's upstream `postinst` uses `xdg-icon-resource` without declaring
`xdg-utils`. Its three AArch64 desktop-file renames are also unconditional, so
a configure retry after the first failure is not idempotent. Installing the
official Debian `xdg-utils` package and re-running the same deb transaction
restored those package-owned files during unpack and then configured cleanly.
No maintainer script or WPS binary was patched.

## One library-resolution rule

The first cold launch found WPS's genuine Qt xcb plug-in but failed to load
`libQt5XcbQpaKso.so.5`. The SONAME existed in `office6`; the problem was that
normalization had converted the main executable's transitive legacy RPATH into
non-transitive RUNPATH. Restoring transitive RPATH globally was rejected: it
previously allowed WPS's bundled FreeType to contaminate Debian Fontconfig.

The single ELF normalizer now indexes shared objects in the rootfs. For each
ELF, it first tests the fixed system directories and relocated original
RUNPATH. If a direct `DT_NEEDED` remains unresolved and exactly one provider
exists in the rootfs, the provider's directory is appended to that ELF's own
RUNPATH. Multiple providers are never guessed. The device ledger consequently
records only this direct edge for the WPS plug-in:

```text
/opt/kingsoft/wps-office/office6/qt/plugins/platforms/libqxcb.so
  DT_NEEDED libQt5XcbQpaKso.so.5
  /data/user/0/io.taowen.bx/files/rootfs/opt/kingsoft/wps-office/office6
```

The Debian Qt6 xcb plug-in retained only the system RUNPATH. A controlled
nested vendor plug-in regression covers unique resolution while the existing
test continues to require `DT_RPATH` to become `DT_RUNPATH`.

## Functional result and open gaps

An untraced cold start reached the complete WPS Writer home UI, opened a real
`Document1`, and accepted two lines of Android keyboard input:

```text
BionicX_WPS_shared_rootfs_2026
Direct_DT_NEEDED_resolution_passed
```

WPS's startup self-check still reports missing CUPS and formula-symbol fonts.
The SaveAs dialog opened in the profile's private Documents directory, but
after dismissing a duplicate-name confirmation its pointer/keyboard grab was
left on the wrong window: controls still hovered while subsequent clicks and
keys were not delivered. The new document therefore was not saved, and this
run is not claimed as full WPS acceptance. The pre-existing `BionicX.docx` was
not overwritten.

Evidence is in `evidence/rebuild-2026-08-13/wps-*`, including the successful
home/editor screenshots, Qt plug-in diagnosis, package status and empty dpkg
audit. No Frida, PRoot, Termux, root runtime, `LD_LIBRARY_PATH`, application
preload, library copy, or WPS-specific resolver branch was used.
