# Architecture

## 1. Runtime layout

Every path exposed to a profile is app-private:

```text
files/
  bin/bionicx-exec
  lib/libbionicx-<module>.so
  profiles/active.json
  apps/<id>/                 ${APP}
  homes/<id>/                ${HOME}
  run/<id>/                  ${TMP}
  rootfs/                    ${RUNTIME}
```

Application and runtime trees are separate so several applications can share
one audited glibc closure while keeping their configuration and mutable home
directories isolated from each other.

## 2. ELF handoff

`bionicx-exec` is an Android NDK/Bionic AArch64 executable. It supports:

- `loader`: exec glibc `ld-linux-aarch64.so.1` explicitly with a controlled
  `--library-path`. This avoids changing the target ELF and is the default for
  portable bundles.
- `direct`: exec the application and let its `PT_INTERP` select glibc. This
  preserves the application's identity in `/proc/self/exe`, but the interpreter
  must be relocated to an absolute, executable app-private path.

The executor forks a child, observes only the exec boundary with ptrace,
single-steps the loader entry, suppresses the synthetic Android SIGSEGV seen on
affected devices, and detaches. It then waits as a normal process supervisor.
There is no ongoing debugger or syscall-emulation dependency.

Environment variables such as `LD_LIBRARY_PATH` and `LD_PRELOAD` are set in the
child after the Bionic executable has started. This prevents Android's linker
from accidentally consuming glibc loader configuration.

## 3. Profile contract

Profiles are versioned JSON documents. The Android host validates the ID,
launch mode, environment names, DPI, socket mode, and compatibility module
names. Values may use `${FILES}`, `${APP}`, `${RUNTIME}`, `${HOME}`, `${TMP}`,
and `${DISPLAY}`.

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

## 5. Compatibility modules

Compatibility belongs at observable ABI boundaries and is opt-in. The WPS
module currently provides process-local SysV semaphores and Android-aware
`popen/pclose`. A new application should not inherit it unless traces prove it
needs those semantics.

Future modules can cover narrow filesystem, IPC, or desktop-service gaps, but
the project does not aim to translate arbitrary Linux syscalls.

## 6. Desktop-service dispatch

Linux GUI toolkits commonly delegate URI opening to a program such as
`xdg-open`. Profiles can prepend an app-private directory to `PATH` and map
file classes to explicit handlers through `bionicx-open`. The dispatcher is a
glibc ELF, not a shell script: a Bionic shebang interpreter cannot safely start
after a glibc GUI process has exported its loader search path.

The current dispatcher decodes local file URIs and supports the configured PDF
handler. It executes the handler inside the same app UID and display session.
Additional MIME classes should be added with controlled integration clients;
BionicX does not pretend to supply a complete freedesktop MIME/D-Bus session.
