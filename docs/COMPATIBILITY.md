# Compatibility model

## What is reusable today

- AArch64 ET_DYN glibc loader handoff from a Bionic app process.
- Direct or explicit-loader execution modes.
- Per-application argv, cwd, environment, library path, preload modules, home,
  temporary directory, X11 transport, and DPI.
- Recursive `DT_NEEDED` auditing and closure copying.
- Core X11 rendering/input sufficient for the Xlib hello client and WPS Writer.
- Core asynchronous `GrabKeyboard`/`UngrabKeyboard`, including cross-client
  contention, owner-events routing, and disconnect cleanup. Synchronous grab
  modes still require `AllowEvents` freeze/thaw support and are rejected rather
  than reported as a false success; non-`CurrentTime` ordering is likewise
  rejected until the server has a timestamp model.
- Core asynchronous passive `GrabKey`/`UngrabKey`, including exact or wildcard
  modifier matching, ancestor activation, automatic release, conflict checks,
  and client/window cleanup.
- Core asynchronous passive `GrabButton`/`UngrabButton`, including exact or
  wildcard button/modifier matching, ancestor activation, owner-events routing,
  automatic release, conflict checks, and client/window cleanup. Synchronous
  pointer mode and non-None confine/cursor overrides remain explicit errors.

## What an application bundle must supply

- The application and every ELF/plugin it may load.
- A mutually compatible glibc loader and shared-library closure.
- Fonts, locales, icon themes, MIME data, and other data files it assumes.
- Relocation of absolute interpreters, socket prefixes, helper paths, and FHS
  paths that cannot exist in the Android app sandbox.

## Kernel and Android constraints

BionicX does not hide Android. Applications observe the device kernel, SELinux
domain, app seccomp filter, page size, and Android filesystem. In particular:

- SysV IPC may be blocked for app UIDs.
- Unmodified Debian glibc 2.41 calls AArch64 `set_robust_list` (syscall 99)
  during startup and receives `SIGSYS` under the tested Android 14 app seccomp
  policy. The reproducible hello bundle therefore uses the pinned
  Android-compatible Winlator glibc 2.39 runtime. BionicX does not hide this
  requirement with long-lived ptrace syscall emulation.
- `/bin/sh`, `/tmp`, `/proc` details, D-Bus, systemd, and desktop portals may
  differ or be absent.
- Executing extracted app-data files currently relies on the experimental APK's
  target SDK 28 behavior. A modern target-SDK product needs a code-loading and
  packaging design compatible with current Android policy.
- Every ELF must be checked on 16 KiB page-size devices; a 4 KiB result does not
  imply 16 KiB compatibility.

## Current X server boundary

The embedded server intentionally disables extensions whose implementation was
not complete enough for Qt (notably MIT-SHM and SYNC in this POC). Applications
requiring GLX, advanced input methods, clipboard integration, accessibility,
or a complete desktop session need additional work and tests.

## Qualification checklist

For each new profile:

1. Audit all entry points and plugin directories with `resolve-elf-deps.py`.
2. Record ELF class, machine, interpreter, page alignment, NEEDED closure, and
   hashes.
3. Run first in diagnostic signal mode only when needed; remove tracing from
   the final launch.
4. Exercise open/edit/save, dialogs, keyboard, pointer, resize, and clean exit.
5. Verify that no root, PRoot, Termux, or debugger process remains in the final
   runtime chain.
