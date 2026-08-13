# Direct-only rootfs contract and WPS acceptance

The previous architecture still exposed two ELF handoff modes and retained
host builders which installed/split complete Chrome and WPS dependency trees.
That allowed profiles and example builders to create different Linux
personalities even after compatibility preloads had been unified.

The contract is now intentionally singular:

- schema 3 has no `mode`, `loader`, `libraryPath`, compatibility or preload
  controls;
- `bionicx-exec` only executes the target ELF directly;
- the host produces a package-manager/runtime seed, with no desktop app;
- device `bxapt` owns the one dpkg database and atomically normalizes every ELF
  carrying `PT_INTERP` after package transactions;
- legacy transitive `DT_RPATH` is normalized to `DT_RUNPATH`, and the global
  search order is Debian system libraries followed by optional controlled
  fixture libraries and the entrypoint directory;
- the same normalization is applied to controlled `${APP}` fixtures during
  profile installation.

## Runtime gaps exposed by real WPS

Direct execution first exposed an old bundled FreeType through WPS's
transitive `DT_RPATH`, which made Debian Fontconfig fail on
`FT_Done_MM_Var`. The generic RUNPATH normalization fixed the namespace rather
than adding a WPS library override.

Android then trapped AArch64 syscall 190 (`semget`). The mandatory runtime now
provides one app-private, file-backed System V semaphore namespace with fcntl
operation locking and shared futex waits. A forked controlled client verifies
cross-process state before WPS consumes it.

Finally, glibc's internal `popen` path reached Android `/system/bin/sh` as
`/bin/sh`, inheriting glibc loader variables and crashing in Bionic linker
IFUNC relocation. The unified FHS exec layer now implements `popen` and
`system` through the normalized Debian rootfs shell. Controlled output and exit
status checks pass. A subsequent WPS cold start produced no new linker
tombstone.

## Device acceptance

Device `01408BH601027129`, ordinary `io.taowen.bx` app UID:

- the rootfs ledger contained 684 normalized entries, including WPS and Chrome
  main/child executables;
- the runtime contract and ELF normalizer host tests passed;
- a controlled Xlib hello bundle was normalized on the device and launched
  direct-only with glibc 2.41;
- WPS Writer reached its full home UI and a new editable `Document1` window;
- no Frida, ongoing ptrace, root, PRoot, Termux, app-specific preload, explicit
  loader mode or software-rendering environment fallback was used.

One short-lived WPS-owned session child still aborts with a glibc allocator
diagnostic while Writer and `wpscloudsvr` remain alive. Its executable identity
must be captured and reduced before WPS is called perfect; it is not hidden by
the acceptance result above.
