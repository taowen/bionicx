# Robust mutex owner death under Android seccomp

## Symptom

The `runtime-probe` owner thread could lock a process-private robust mutex and
exit, but the joining thread then blocked forever instead of receiving
`EOWNERDEAD`.  The isolated test's alarm converted that hang into the stable
result:

```
BXTEST FAIL pthread-robust-mutex signal=14 owner-death timeout
```

The same device reports `SIGSYS` for a direct AArch64 `set_robust_list`
(syscall 99).  This is the Android app zygote seccomp policy, not an X11 issue.

## Root cause

The Winlator-derived glibc 2.39 runtime deliberately removes the startup and
per-thread `set_robust_list` calls so that an ordinary Android app process is
not killed by seccomp.  Its package recipe was traced to commit
`e2ffc0bb462177386b44ec66e30e6e939d846871` in
`termux-pacman/glibc-packages` (the last 2.39 recipe before the 2.41 update).

glibc already contains a userspace owner-death fallback in
`nptl/pthread_create.c`, but it is compiled only when
`__ASSUME_SET_ROBUST_LIST` is absent.  AArch64 defines that macro
unconditionally in `sysdeps/unix/sysv/linux/kernel-features.h`.  Deleting the
syscall alone therefore avoids `SIGSYS` while also compiling out the only
replacement notification path.

## Fix

`runtime/glibc/2.39/zz-bionicx-robust-fallback.patch` makes the Android glibc
build stop assuming kernel robust-list registration and records registration
as unavailable.  This enables glibc's existing normal thread-exit path, which:

1. walks the exiting thread's held robust mutex list;
2. atomically sets `FUTEX_OWNER_DIED`; and
3. performs `futex(FUTEX_WAKE, 1)`.

AArch64 object-code inspection confirmed the fallback in `start_thread`: an
OR of `0x40000000`, followed by syscall 98 with a wake count of one.

The fallback cannot reproduce kernel cleanup after abrupt process death, and
glibc's implementation was designed only for ordinary process-private,
non-PI mutexes.  The patch therefore makes robust process-shared and robust PI
initialization return `ENOTSUP` when kernel registration is unavailable,
rather than claiming semantics it cannot provide.

The old package helper also needed a typed adapter for its fake `close_range`
dispatch table to compile with GCC 14.  That mechanical portability fix is in
`runtime/glibc/2.39/post-prepare-gcc14.patch`; it does not change robust mutex
behavior.

## Sources

- GNU glibc 2.39 tag, commit `ef321e23c20eebc6d6fb4044425c00e6df27b05f`
- termux-pacman glibc package recipe, commit
  `e2ffc0bb462177386b44ec66e30e6e939d846871`
- Linux `kernel/futex/core.c`, `exit_robust_list` / `handle_futex_death`

## Device acceptance

On x300 `01408BH601027129`, the unchanged probe and APK were run with only
loader/libc/libm replaced by the patched build.  The direct syscall remained
blocked, while owner death recovered immediately:

```
BXTEST PASS pthread-robust-mutex isolated-exit=0
BXCAP raw-set-robust-list signal value=31 (Bad system call)
BXSUMMARY runtime passed=20 failed=0
```

The accepted libc has build ID
`839cab80f4d69a3b52addebf01701ada7daa3795` and SHA-256
`bf6f6b184710068d1766d95b46d8fa3c578ef2a4da4a0d932f9a8c92bc97c4ee`.
This test used the ordinary untraced executor; no Frida, ptrace, proot, or
Termux process participated.  The complete filtered device log is retained in
`evidence/runtime-probe-robust.log`.
