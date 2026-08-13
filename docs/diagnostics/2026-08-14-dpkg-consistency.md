# Declared-package dpkg consistency

Device `01408BH601027129`, seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.

All 31 declared names (`packages/trixie-popular.txt`,
`packages/external-arm64.tsv`, plus `cups-daemon`/`cups-client`) are
`install ok installed`, including hash-pinned Chrome, WPS, `libwebp6`
and `libtiff5`. `bxapt dpkg --audit` is empty before and after each
step. `find files/apps` has no `libc.so.6`, loader, or `libstdc++.so.6`.

`bxapt install --reinstall bsdextrautils` reinstalled one leaf
(3753 ELF fixups). `bxapt remove ristretto` left the other 30 declared
packages `ii` and audit empty. `bxapt set packages/trixie-popular.txt`
put ristretto back to `ii`. One dpkg database, no per-app system
libraries.

Evidence: `evidence/rebuild-2026-08-14/dpkg-consistency.log`.
