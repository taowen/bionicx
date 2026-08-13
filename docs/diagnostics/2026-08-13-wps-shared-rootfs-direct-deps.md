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

## Functional result

An untraced cold start reached the complete WPS Writer home UI, opened a real
`Document1`, and accepted two lines of Android keyboard input:

```text
BionicX_WPS_shared_rootfs_2026
Direct_DT_NEEDED_resolution_passed
```

The document was saved as a genuine OOXML `BionicX.docx`. The independent
archive verifier found 16 members and two paragraphs containing the exact
strings above. After a process stop and cold start, WPS's own Open dialog
opened the saved file and rendered both lines again. The test artifact was
then retained as `BionicXSharedRootfs.docx`; the pre-existing `BionicX.docx`
was not overwritten.

An earlier attempt selected that pre-existing filename. WPS rejected the
duplicate and its dialog flow became awkward, but the request log contained
neither a Core X11 nor XI grab request. Repeating the same SaveAs operation in
an empty Documents directory passed. That observation is therefore not used
as evidence for an X server focus/grab gap, and no X server workaround was
added.

WPS's startup self-check still reports missing CUPS and formula-symbol fonts;
these are tracked separately from the completed editor/save/reopen path.

## What the CUPS warning actually means

The warning is not evidence that Debian 13's CUPS ABI is too new. Disassembly
of `KCUPSSupport::resolveCups()` shows WPS constructing `QLibrary("cups", 2)`
and resolving 27 CUPS, IPP, and PPD entry points. Every requested entry point
is exported by trixie's `libcups.so.2`, and the glibc loader resolves that
library and all of its dependencies from the shared rootfs.

Installing `libcups2-dev` added the unversioned development link but did not
change the warning, as expected from the explicit ABI version in the WPS
code. `KCUPSSupport::isInitSuccess()` also requires initialization to have
returned at least one printer destination. The old WPS self-check collapses
the no-service/no-destination state into the misleading text “Missing Cups
libraries.” The remaining work is consequently a shared desktop printing
service and destination contract, not a libcups downgrade or binary patch.

The formula warning has a similar legacy component: this WPS build contains
explicit references to `Cambria Math` and the Wingdings families. Installing
the normal Debian Symbola, Noto, and Liberation font set does not provide
those proprietary family identities. Font substitution and glyph coverage
must be tested separately; suppressing the warning is not acceptance.

Evidence is in `evidence/rebuild-2026-08-13/wps-*`, including the successful
home/editor screenshots, Qt plug-in diagnosis, package status and empty dpkg
audit. No Frida, PRoot, Termux, root runtime, `LD_LIBRARY_PATH`, application
preload, library copy, or WPS-specific resolver branch was used.
