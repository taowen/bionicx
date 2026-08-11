# Fatal-signal ELF mapping report

## Motivation

The first untraced attempt to create a WPS Spreadsheets workbook ended with
exit status 139.  The existing short-lived `diagnoseSignals` mode could report
the AArch64 PC and LR, but ASLR made those absolute addresses insufficient to
identify the responsible ELF without a second, timing-sensitive `/proc` read.

## Change

While a fatal signal is stopped under `ptrace`, `bionicx-exec` now records:

- AArch64 registers `x0` through `x7` and `sp`, in addition to PC and LR;
- the exact `/proc/<pid>/maps` row containing PC, LR, and the fault address;
- the ASLR-independent ELF file offset computed as mapping offset plus the
  address displacement within that mapping.

The normal launch path remains unchanged and detaches immediately after its
loader bootstrap.  Mapping collection occurs only when an explicitly enabled
diagnostic run has already stopped on a fatal signal.

## Device verification

On x300 `01408BH601027129`, API 34, the new executor was built with the Android
NDK under `-Wall -Wextra -Werror`, packaged, and installed.  A controlled
Bionic `/system/bin/sh` child raised `SIGSEGV` under the executor:

```sh
adb -s 01408BH601027129 shell \
  "run-as io.taowen.bx files/bin/bionicx-exec --diagnose-signals \
  --direct -- /system/bin/sh -c 'kill -SEGV \$\$'"
```

The executor returned 139 and identified PC in Android `libc.so` at file offset
`0xae61c` and LR in `/system/bin/sh` at file offset `0x28860`.  The retained
output is in `evidence/signal-elf-mappings.log`.

The WPS diagnostic rerun did not reproduce the earlier crash: the same New
Document action reached a genuine empty `Book1` grid.  Therefore no workaround
is inferred from the one prior exit; workbook behavior must be verified again
in the final untraced profile.
