# Runtime SIGTRAP diagnosis

## Failure mode

After GTK became loadable, Chrome deliberately executed an AArch64 `brk #1`
during native-UI initialization. The ordinary untraced profile correctly
exited with signal 5, but `bionicx-exec --diagnose-signals` suppressed every
SIGTRAP as though it were one of the executor's bootstrap single-step stops.
The tracee remained on the same instruction and consumed a CPU core, hiding
the register and ELF mapping that diagnostic mode exists to collect.

## Correction

The executor finishes its exec event, loader probe, and optional loader retry
before entering `diagnose_signals()`. A SIGTRAP observed inside that later loop
is therefore an application breakpoint/crash, not a bootstrap event. It now
uses the same bounded report-and-terminate path as SIGSEGV, SIGSYS, SIGILL,
SIGBUS, and SIGABRT.

## Chrome proof

The next diagnostic launch terminated immediately and retained the decisive
signal, registers, and file-relative addresses in
`evidence/chrome-gtk-sigtrap-diagnose.log`:

```text
bionicx-exec: signal=5 syscall=15 pc=0x6062f65ecc lr=0x6062f65d94 code=1 address=0x6062f65ecc
bionicx-exec: pc mapping=... /data/user/0/io.taowen.bx/files/apps/chrome/opt/google/chrome/chrome file-offset=0xa725ecc
```

The corresponding executable offset disassembles to `brk #0x1`. This is a
diagnostic-only launch; the tracked Chrome profile remains untraced and does
not enable signal diagnosis.
