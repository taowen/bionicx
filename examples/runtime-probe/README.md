# glibc/kernel/X11 runtime probe

This genuine AArch64 glibc application exercises process and IPC primitives
needed beneath Chromium-class GUI software, then creates a real libX11 window.

Strict `BXTEST` checks cover pthreads and robust mutex recovery, fork/wait,
eventfd/epoll, timerfd, signalfd, memfd/shared mmap, mprotect, SCM_RIGHTS,
filesystem Unix sockets, loopback TCP, inotify, procfs, dlopen, getrandom,
unnamed semaphores, locale and prctl.

Potentially fatal or platform-optional operations run in disposable children
and emit `BXCAP`: raw `set_robust_list`, user namespaces, SysV shared memory,
and POSIX shared memory. A SIGSYS therefore becomes evidence instead of taking
down the diagnostic process.

```sh
examples/runtime-probe/build-bundle.sh
ANDROID_SERIAL=<serial> examples/runtime-probe/install-and-run.sh
adb -s <serial> logcat -d -s BionicX | grep -E 'BX(TEST|CAP|SUMMARY)'
```

`profiles/wps-compat-probe.json` runs the same binary with the WPS compatibility
module and adds an Android-shell `popen` check. It deliberately sets a glibc
`LD_LIBRARY_PATH` in the parent so the compatibility layer must sanitize the
environment before executing Bionic `/system/bin/sh`.
