# Single runtime path

## Problem

The direct-ELF transition still left three overlapping mechanisms:

- the Android launcher and `bxapt` injected a process-wide
  `LD_LIBRARY_PATH`, hiding ELFs that had not been normalized;
- the pinned glibc source recipe disabled Android-blocked `clone3` and
  `set_robust_list` calls, while the runtime also scanned and modified libc
  instructions at startup;
- glibc was built with a fixed rootfs `resolv.conf`, while the runtime also
  replaced `__res_ninit` state directly.

The overlap was observable on the clean seed: removing `LD_LIBRARY_PATH`
initially made a child coreutils executable fail to find `libselinux.so.1`.
Adding the system search path to every dynamic ELF then exposed a second
normalizer bug: rewriting `ld-linux-aarch64.so.1` itself made it crash during
bootstrap.

## One contract

There is now one implementation for each boundary:

- every dynamic ELF except the loader has the fixed rootfs system `RUNPATH`;
- the loader is an independent regular seed file and is never modified by
  `patchelf`;
- the pinned glibc build is the only implementation of blocked libc syscall
  behavior, with a build-time binary check that rejects raw `clone3` or
  `set_robust_list` stubs;
- Android supplies current DNS addresses, the runtime atomically publishes
  one rootfs `resolv.conf`, and unhooked glibc reads its compiled path;
- neither the Activity nor `bxapt` exports `LD_LIBRARY_PATH`.

There is no compatibility branch or fallback to the previous behavior.

## Acceptance

On device `01408BH601027129`, under the ordinary `io.taowen.bx` UID:

- the rebuilt Debian 13 seed normalized 1,008 ELF entries;
- `getent ahostsv4 snapshot.debian.org` resolved without
  `LD_LIBRARY_PATH`;
- real rootfs `apt-get update` fetched 10.0 MB of signed snapshot indices with
  no failed-fetch or DNS warning;
- the untraced runtime probe passed all 20 tests, including pthread creation,
  robust owner death, process/IPC/network primitives and a real X11 window;
- startup emitted no runtime libc-instruction adaptation warning.

Evidence is in `evidence/rebuild-2026-08-13/direct-runpath-*.log`.
