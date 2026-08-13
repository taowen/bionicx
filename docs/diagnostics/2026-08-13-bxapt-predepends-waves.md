# bxapt must configure Pre-Depends parents before the next unpack wave

Reconstructing the shared Debian 13 seed plus `packages/trixie-popular.txt`
failed at `dpkg --unpack` of the whole archive set:

- `libreoffice-common` Pre-Depends on `ucf`
- `python3-minimal` / `python3` Pre-Depends on `python3.13-minimal`
- `systemd` / `systemd-sysv` Pre-Depends on `libsystemd-shared`

Debian will not unpack a package whose Pre-Depends are only unpacked. The
previous one-shot `dpkg --unpack archives/*.deb` therefore left those children
uninstalled (`in`) and hundreds of siblings half-installed (`iU`). A later
`apt-get -f install` cannot finish them without unpacking the children, and
letting apt unpack+configure would skip ELF normalization.

`bxapt` now unpacks in waves: unpack what Pre-Depends currently allow, run
`rootfs-elf-fixup.sh` on that wave, `dpkg --configure -a --abort-after=10000`,
then retry the remaining archives. `tests/test-bxapt-unpack-waves.sh` mocks
that contract without a device.

A failed one-shot unpack also skipped ELF normalization, so helpers such as
`/usr/bin/python3.13` and `fc-cache` still had PT_INTERP
`/lib/ld-linux-aarch64.so.1` and maintainer scripts reported "not found".
`bxapt recover` now runs a full-tree fixup before configure.

Debian `ldconfig` is a static PIE, so `ldconfig -r "$DPKG_ROOT"` cannot use
the runtime `chroot` interposer and a bare `ldconfig` writes Android `/etc`.
`tools/bxapt-ldconfig.sh` replaces `/sbin/ldconfig` with a wrapper that writes
`$BIONICX_ROOTFS/etc/ld.so.cache`. Covered by
`tests/test-bxapt-ldconfig-wrapper.sh`.

Python 3.13 `subprocess` uses `posix_spawn("/usr/bin/dpkg")`, which bypassed
`execve` interposition and looked at the Android path. The runtime now
redirects `posix_spawn` / `posix_spawnp` and `readlink` / `readlinkat`.
`tests/test-runtime-contract.sh` covers both.

The seed ships `systemd-standalone-sysusers`. Unpacking full `systemd`
conflicts with that virtual package; `bxapt` removes the standalone package
when a `systemd_*.deb` is in the archive set.
