# Debian trixie Geany, FileZilla and Thunderbird

## Result

The pinned Debian 13 snapshot `20260811T000000Z` now installs three additional
unmodified ARM64 packages into the existing shared desktop rootfs:

- Geany `2.0-2`;
- FileZilla `3.68.1-1`;
- Thunderbird `1:140.13.0esr-2~deb13u1`.

The resulting content ID is
`b7ea6f1f1aac7a71432910ec5bcd78511d4f74870ec490586da1512e5e0da049`.
The builder patched 674 packaged ELF interpreters, relocated 370 executable
script shebangs and 110 absolute ELF runpaths, then verified 2,474 ELF objects
against the GLIBC 2.41 ceiling. The same rootfs was transferred once and reused
for every profile.

All tests ran untraced as Android `u0_a194` on x300 serial
`01408BH601027129`, without root, PRoot, Termux or Frida.

## Workflows

Geany opened the controlled C fixture, rendered syntax highlighting, accepted
Android keyboard input and saved `// Saved on Android by BionicX`. The saved
bytes were read back with `run-as`, rather than inferred from the screenshot.

FileZilla rendered its wxWidgets main window, local directory tree and welcome
dialog. Closing the welcome dialog and opening Site Manager exercised a real
transient dialog and XI2 client-pointer query.

Thunderbird rendered its account-setup workflow, accepted a controlled name and
email address, and retained four live Mozilla processes. Network account
discovery was intentionally not attempted with fake credentials.

Evidence:

- `evidence/bionicx-trixie-geany-edit.png`;
- `evidence/bionicx-trixie-filezilla-site-manager.png`;
- `evidence/bionicx-trixie-thunderbird-setup.png`.

## General gaps exposed

Geany compiles `/usr/share/geany` into the binary and provides no Unix runtime
override. `libbionicx-android-tmp.so` therefore gained an opt-in
`BIONICX_ROOTFS` mapping for ordinary `/usr`, `/etc` and `/var` file APIs. This
keeps the Debian package unchanged and is reusable by other packages with FHS
data paths.

GTK names cursor resources through XFixes `SetCursorName`; BionicX now consumes
and validates that request instead of returning `BadImplementation`. FileZilla
then exposed XI2 `XIGetClientPointer`, which now reports the real synthetic
master pointer. Both additions use their standard wire layouts and are server
capabilities, not application bypasses.

The D-Bus warning was non-fatal and identified a desktop-service task rather
than a per-application workaround. It is resolved by the subsequent
profile-selected session service documented in `2026-08-13-dbus-session.md`.
