# Architecture

## 1. Runtime layout

Every path exposed to a profile is app-private:

```text
files/
  bin/bionicx-exec
  lib/libbionicx-runtime.so
  profiles/active.json
  apps/<id>/                 ${APP}
  homes/<id>/                ${HOME}
  run/<id>/                  ${TMP}
  rootfs/                    ${RUNTIME}
```

Application and runtime trees are separate so several applications can share
one audited glibc closure while keeping their configuration and mutable home
directories isolated from each other.

The device owns one Debian 13 (trixie) rootfs and one dpkg database. Debian
packages and hash-pinned external `.deb` files are installed by the rootfs's
real apt/dpkg through `bxapt`; `/opt`, package data and dependencies remain in
that shared tree. Profiles never carry a dependency closure or private system
library copy.

The Debian snapshot timestamp, base-image digest and external package hashes
are immutable inputs. The host-built image remains the reproducible seed for
CI and large cohorts. On Android, `bxapt` can run the rootfs's real apt and dpkg
as the application UID, using the same signed snapshot, package database,
maintainer scripts and triggers. It adds packages to the shared rootfs instead
of constructing another per-application library closure.

The package transaction has a logical Debian uid/gid 0 while the kernel still
sees the ordinary Android application credentials. In that virtual-root
namespace glibc consults only the rootfs passwd/group databases; Android's uid
fallback remains available to normal application processes but cannot occupy
Debian's system-account range. This identity switch is scoped to apt/dpkg and
is not exported by `bxapt run` or an application profile.

Package installation is split at the real dpkg transaction boundary: apt first
downloads the resolved set, dpkg unpacks it, BionicX normalizes the new ELFs,
then apt configures packages and runs triggers. This ensures maintainer-script
helpers use the same loader contract as applications. The seed includes
Debian's standalone sysusers implementation for rootfs account creation without
a running systemd or Android root privileges.

The unpacker records the paths owned by the exact downloaded deb set. The ELF
normalizer builds its provider index from the complete rootfs, but atomically
replaces ledger entries and rewrites files only for that transaction manifest.
It therefore retains one global dependency namespace without rescanning every
application under `/opt` after each package. `bxapt fixup` is the explicit
full-root audit operation; normal installation does not use it as a fallback.

The explicit unpack boundary means apt does not itself update `extended_states`
for the downloaded dependency set. `bxapt` snapshots installed packages before
the transaction and reconciles only the delta after successful configuration:
new resolver dependencies become automatic and explicitly requested packages
become manual. Removal and autoremove also run the incremental ledger pass,
which prunes records for package-owned ELFs that no longer exist.

This is a deployed userspace layout, not a chroot. Every Debian process enters
one mandatory runtime contract (`libbionicx-runtime.so`) which defines the
Android-kernel, FHS, identity and DNS boundary consistently.

## 2. ELF handoff

`bionicx-exec` is an Android NDK/Bionic AArch64 executable. It always executes
the target directly. `bxapt` atomically normalizes every package-installed ELF
with `PT_INTERP` to the one app-private glibc loader, converts legacy transitive
`DT_RPATH` to `DT_RUNPATH`, and relocates absolute search entries. This keeps
`/proc/self/exe`, child execution and library resolution identical for every
application. Explicit-loader execution is not a profile or executor feature.

The executor forks a child, observes only the exec boundary with ptrace,
single-steps the loader entry, suppresses the synthetic Android SIGSEGV seen on
affected devices, and detaches. It then acts as a Linux child subreaper for the
whole display session. A primary process may exit after spawning detached GUI
children; the executor remains until every adopted descendant exits. There is
no ongoing debugger or syscall-emulation dependency.

Before handoff, ignored termination signals and the inherited signal mask are
reset so glibc children obey normal process semantics. Activity shutdown sends
TERM to the supervisor, which terminates the primary process group and the
single-UID BionicX session, waits briefly, and escalates remaining children to
KILL. One APK process/UID owns one active display session, matching the Android
sandbox and preventing detached clients from outliving their X server.

The normalized `RUNPATH` is the only library-search contract. In addition to
the fixed system directories and the ELF's relocated original entries, the
normalizer records the object's own directory as a concrete path — `$ORIGIN`
expands to the pathname used to open the file, so a DSO reached through a
multiarch symlink would otherwise search the symlink directory instead of its
siblings. That directory is prepended when every colliding system SONAME is
only a symlink back to the same file (LibreOffice `program/`, so `dladdr` and
`getUnoIniUri` see the private tree). It is appended when the directory also
ships a real system SONAME (WPS bundled FreeType), so Debian still wins. The
normalizer also adds the directory of a directly needed SONAME when that
provider is unique in the shared rootfs. This preserves non-transitive
namespace isolation while supporting vendor plug-ins whose direct dependencies
live next to the vendor executable. Ambiguous providers are never guessed. `LD_LIBRARY_PATH`
is neither injected nor accepted, so an incomplete package normalization fails
directly instead of being hidden by a process-global search path. The mandatory
`LD_PRELOAD` is set in the child after the Bionic executor has started, keeping
the Android linker outside the glibc environment.

## 3. Profile contract

Profiles are versioned JSON documents. Schema version 3 validates the ID,
environment names, DPI, socket mode and host services. Values may
use `${FILES}`, `${APP}`, `${RUNTIME}`, `${HOME}`, `${TMP}`, and `${DISPLAY}`.
There is no compatibility-module field: profiles cannot change the runtime
ABI boundary. `LD_PRELOAD`, `LD_LIBRARY_PATH`, `BIONICX_ROOTFS`,
`BIONICX_TMPDIR` and `BIONICX_DNS_SERVERS` are runtime-owned.

The profile is configuration, not an installer. Bundle acquisition, license
acceptance, recursive dependency resolution, interpreter relocation, and
integrity checking happen before activation.

## 4. X11 transport

The glibc application uses its normal libX11/libxcb stack and emits standard
X11 requests. The APK owns the server side and renders through Android
`GLSurfaceView`; Android key/touch events are translated into X input events.

Two Unix-domain transports are supported:

- `abstract`: server address `@/tmp/.X11-unix/X0`; suitable for an unpatched
  Xlib transport that falls back to Linux abstract namespace sockets.
- `filesystem`: `${RUNTIME}/tmp/.X11-unix/X0`; suitable for the Winlator-patched
  libxcb used by the WPS POC.

The X server reports a configurable symmetric DPI. WPS uses 144 DPI on the
1920x1080 test device; the former fixed 254 DPI made Qt dialogs larger than the
root window.

### Host GPU presentation

Software X11 follows the glamor model. One rule: the GLES texture is the
32-bit drawable. There is one GPU path, not a Fast/CPU fill plus a
separate Over cache. Clear/Src fill with `glClear`, CopyArea with
`glBlitFramebuffer`, and Porter-Duff (Src/Over/In/OutReverse/Add/Saturate)
plus A8 glyphs with one shader. `SetPictureTransform` is that same
shader, not a second size-keyed path. Missing X requests log
`BXINFO unimplemented` on `BionicX:I` (accept scripts collect that tag;
`WinlatorXRequest:W` / `BadImplementation` alone is invisible). The X request waits for that GPU work
(GLSurfaceView thread). One request that names many rects
(PolyFillRectangle, clipped CopyArea, tiled backgrounds) is one hop. The CPU `ByteBuffer` is a GetImage /
`prepare_access` cache: every CPU read (GetImage, `pictureColor`, clip
masks) downloads first, and a GPU write does not upload stale heap bytes
back over the texture. GetImage reads only the requested rectangle;
PutImage of 24/32-bit pixels uploads that rectangle instead of
reading the rest of the drawable back first. `glReadPixels` (BGRA, no
swizzle) runs only then, not after every Composite. 24-in-32 unused alpha is treated as opaque.
Present samples the textures and `tryLock`s `DRAWABLE_MANAGER` so a
download cannot deadlock against vsync. A8 dests, gradients and
component-alpha stay on the CPU after a download.

A second path remains for native GL clients: DRI3/Present uses `GPUImage`,
backed by Android `AHardwareBuffer` and imported as `EGLImageKHR`. Mali
will not sample CPU stores into an AHB that stays bound as an EGLImage
FBO, so software Render dests do not use that backing. CPU pixman-style
fallback stays for formats and ops the GLES path does not implement.

## 5. Runtime contract

`libbionicx-runtime.so` is mandatory for applications, D-Bus and package
helpers. Its implementation is split only for source ownership:

- `android-kernel.c` defines Android seccomp/syscall behavior;
- `fhs-path.c`, `fhs-exec.c` and `fhs-metadata.c` map the single shared rootfs;
- `identity.c` exposes the current Android app UID/GID to glibc software;
- `dns.c` atomically publishes the Android network's active resolvers to the
  rootfs path compiled into glibc; it does not hook glibc's resolver.
- `sysv-semaphore.c` supplies one app-private, file-backed System V semaphore
  namespace with cross-process locking and futex waits because Android seccomp
  blocks the corresponding AArch64 syscalls for ordinary app UIDs.

These are not selectable plugins and cannot vary by application. Unsupported
kernel capabilities are explicit. In particular robust pthread mutexes return
`ENOTSUP`, while ordinary pthread creation is required and integration-tested.
Application-specific preload libraries and fallback module names are absent.

## 6. Desktop services

Profiles opt into app-private desktop daemons through `hostServices`. The
`dbus` service starts Debian's unmodified `dbus-daemon` with the session
configuration shipped in the shared rootfs and publishes its Unix socket at
`${TMP}/runtime/bus`. BionicX injects the matching
`DBUS_SESSION_BUS_ADDRESS`; every glibc process in that profile therefore uses
one real session bus without PRoot, Termux, a system daemon, or a second copy of
the Debian packages. The daemon has the same Android UID and lifecycle as the
X11 session. A stale socket left by Android force-stop is removed before the
next daemon starts.

This supplies transport, name ownership and Debian's normal activation lookup.
It does not claim that every optional desktop service is installed or usable:
portals, notifications, secrets and accessibility daemons require separate
controlled tests before profiles can depend on them.

### URI dispatch

Linux GUI toolkits commonly delegate URI opening to a program such as
`xdg-open`. Profiles can prepend an app-private directory to `PATH` and map
file classes to explicit handlers through `bionicx-open`. The dispatcher is a
glibc ELF, not a shell script: a Bionic shebang interpreter cannot safely start
after a glibc GUI process has exported its loader search path.

The current dispatcher decodes local file URIs and supports the configured PDF
handler. It executes the handler inside the same app UID and display session.
Additional MIME classes should be added with controlled integration clients;
BionicX does not pretend to supply a complete freedesktop portal environment.
