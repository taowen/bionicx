# glibc/kernel runtime baseline on x300

## Identity

- Device: `01408BH601027129`, Android 14/API 34, AArch64, 4 KiB pages
- Client: genuine AArch64 glibc 2.39 + libX11
- Execution: normal `io.taowen.bx` app UID, untraced after ELF handoff

## Strict results

Seventeen checks passed: ordinary pthread create/join, fork/wait,
eventfd+epoll, timerfd, signalfd, memfd shared mappings, mprotect, Unix
SCM_RIGHTS, filesystem Unix sockets, inotify, procfs, dlopen, getrandom,
unnamed semaphores, prctl naming, and a rendered X11 window.

Three checks failed reproducibly:

```text
BXTEST FAIL pthread-robust-mutex signal=14 owner-death timeout
BXTEST FAIL tcp-loopback socket errno=Operation not permitted
BXTEST FAIL locale-c-utf8
BXSUMMARY runtime passed=17 failed=3
```

The TCP failure occurs at the initial `socket(AF_INET, ...)`, before bind or
connect. The APK currently has no Android `INTERNET` permission; this is the
next isolated correction.

The runtime bundle contains no locale archive or locale tree, so `C.UTF-8`
cannot currently be established. This must be fixed as bundle/runtime data,
not hidden by changing the test to plain `C`.

## Isolated platform capabilities

```text
BXCAP raw-set-robust-list signal value=31 (Bad system call)
BXCAP user-namespace denied value=22 (Invalid argument)
BXCAP sysv-shm signal value=31 (Bad system call)
BXCAP posix-shm denied value=2 (No such file or directory)
```

Raw `set_robust_list` and SysV shm are killed by Android app seccomp. POSIX shm
expects `/dev/shm`, which Android does not provide here. User namespaces are
unavailable. These are capability observations, not blanket test failures.

The robust-mutex deadlock is consistent with an Android-compatible glibc that
avoids the forbidden registration syscall: when the owner thread exits, the
kernel cannot mark the mutex owner-dead. That relationship is a source-guided
inference and still needs a deliberate compatibility design before correction.

Evidence: [runtime-probe-baseline.png](../../evidence/runtime-probe-baseline.png),
SHA-256
`30491add375eb7fefc69ed7bfdc25465864959edc19da340388a886881febc08`.
