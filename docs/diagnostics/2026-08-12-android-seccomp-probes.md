# Android seccomp optional-syscall probes

## Symptom

Chrome 151 reached its Linux startup path under `run-as` tracing, but an ordinary
Android Activity launch died with `SIGSYS`. Frida was not involved in the final
run and is not a runtime dependency.

An opt-in, async-signal-safe reporter recorded two successive traps on the x300:

- AArch64 syscall 444, `landlock_create_ruleset`;
- AArch64 syscall 264, `name_to_handle_at`.

Both are feature probes for optional desktop-Linux facilities. A normal Linux
kernel without those facilities reports `ENOSYS`, which lets the caller select a
fallback. Android's app seccomp policy instead delivers `SIGSYS` before glibc can
return an error.

## Correction

`libbionicx-android-seccomp.so` installs a targeted `SA_SIGINFO` handler. On
AArch64 it recognizes only those two syscall numbers, writes `-ENOSYS` into
register `x0` in the saved `ucontext`, and resumes after the trapped `svc`.
Unknown traps are logged and terminate with status 159.

This does not remove, replace, or weaken Android's seccomp filter. It translates
two known optional probes into the same observable result as an unsupported
Linux kernel. The module deliberately does not interpose the variadic glibc
`syscall()` function, because forwarding an unknown argument count is undefined
and changes process-wide symbol behavior.

The separate `sigsys-report` module remains available for discovering the next
blocked syscall and always exits after reporting. Raw observations are retained
in `evidence/chrome-android-seccomp-probes.log`.
