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

All probes use the mandatory runtime contract. Android does not provide glibc
robust-list owner-death semantics, so robust mutex setup is reported as
`BXCAP ... unavailable` and returns `ENOTSUP`; it is not emulated.

```sh
examples/runtime-probe/build-bundle.sh
ANDROID_SERIAL=<serial> examples/runtime-probe/install-and-run.sh
adb -s <serial> logcat -d -s BionicX | grep -E 'BX(TEST|CAP|SUMMARY)'
```

`profiles/wps-compat-probe.json` is retained as a controlled command-launch
fixture, but it uses the same runtime contract as every other profile.
