# Rootless package identity boundary

Installing trixie's `cups-daemon` and `cups-client` exposed two independent
identity failures in Debian maintainer scripts. First, chrootless dpkg did not
inherit the logical root state established by BionicX's virtual `chroot()`.
`adduser` therefore rejected the real Android app uid. `bxapt` now exports
`BIONICX_VIRTUAL_ROOT=1` only to apt/dpkg transactions; normal applications and
`bxapt run` retain their real uid/gid view.

After that correction, `getent group 148` crashed. The pinned Android glibc
adaptation assigned `group.gr_mem` to the group-name bytes instead of a member
pointer array. Upstream glibc-packages fixed this in commit
`fd2ae25e04f3ea26d6c7b4678020814889331d86`. Its complete newer recipe no
longer applies to the fixed glibc 2.41 source, so BionicX carries that exact
source fix as a documented backport.

The same probe revealed a namespace conflict: the Android NSS fallback treated
all low numeric ids as Android identities, occupying Debian's complete
100-999 system-account allocation range. The backport disables only that
fallback while `BIONICX_VIRTUAL_ROOT=1`; rootfs `/etc/passwd` and `/etc/group`
remain authoritative in a package transaction. The QEMU AArch64 regression
test verifies a real Android app group and safely enumerates its member list,
then verifies that both low and app Android groups disappear in virtual-root
mode. The x300 device independently returned
`u0_a148::10148:u0_a148` in normal mode and no entry for GID 148 in the Debian
namespace.

The CUPS transaction then selected free Debian GID 101 correctly but exposed a
separate shadow-tools `/etc/group.lock` creation failure. That path issue is not
fixed here. To leave the device package database consistent, the already
standard `systemd-sysusers --root` mechanism created `lpadmin` and `ssl-cert`,
after which unmodified package postinst scripts completed. Final state is:

```text
ii  cups-client 2.4.10-3+deb13u2
ii  cups-daemon 2.4.10-3+deb13u2
ii  ssl-cert    1.1.3
```

`dpkg --audit` is empty. The two requested CUPS packages are manual and their
eight newly introduced dependencies are automatic. WPS cold-started with PID
8835 after the new libc was deployed and rendered its complete home window.
The CUPS warning remains expected because this checkpoint intentionally did
not start cupsd or create a printer destination.

Evidence is under `evidence/rebuild-2026-08-13/` with the
`android-glibc-identity-*`, `bxapt-cups-*`, and
`wps-after-glibc-identity-fix.*` names. No root runtime, PRoot, Termux, Frida,
package script modification, WPS branch, or identity fallback was added.
