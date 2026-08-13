# Rootless on-device apt/dpkg (`bxapt`)

## Outcome

The x300 `01408BH601027129` updated the signed Debian 13 ARM64 snapshot,
installed, ran, removed, and reinstalled `hello`, then installed and configured
`x11-apps` plus its six new dependencies. Everything ran as the ordinary
`io.taowen.bx` application UID. Root was not used by `bxapt`; it was used once
to correct the test device's stale December 2025 system clock so August 2026
repository signatures could be validated.

The accepted repository was the rootfs-pinned snapshot:

```text
http://snapshot.debian.org/archive/debian/20260811T000000Z trixie
http://snapshot.debian.org/archive/debian-security/20260811T000000Z trixie-security
```

`apt-get update` verified both InRelease files with Debian's `sqv` and archive
keyrings, then downloaded the ARM64 Packages indexes. No trusted-local or
allow-unauthenticated override was used.

## Why a thin launcher

`bxapt` is not another package manager. It uploads only an apt configuration
whose state, cache, sources, methods, keyrings, dpkg database and install root
all point into the app-private shared rootfs. The actual solver and installer
are Debian apt 3.0.3 and dpkg 1.22.22. Android DNS is passed to the existing
glibc resolver compatibility module.

The distro's `apt.conf.d` is intentionally not loaded. Host-oriented hooks such
as `dpkg-preconfigure --apt` assume a controlling terminal and a host `/bin/sh`;
debconf is instead set to its supported noninteractive frontend while dpkg
continues to run package maintainer scripts and triggers.

## Compatibility gaps exposed

This test added general app-private FHS behavior to `android-tmp`:

- `mkstemp`, `mkostemp`, their suffix variants, `mkdtemp`, and `chdir` redirect
  literal `/tmp` paths. apt needs these for detached signature inputs.
- `execv`, `execve`, `execvp`, and `execlp` redirect absolute FHS helpers and
  search the rootfs when dpkg resets PATH. This allowed apt's hard-coded
  `/usr/bin/sqv` and dpkg's `dpkg-split` helper to execute.
- Script execution parses the real shebang and starts the corresponding rootfs
  interpreter, including optional arguments such as `/usr/bin/perl -w`. Newly
  downloaded maintainer scripts therefore need no package mutation.
- `chmod` and `chown` families redirect FHS paths. Ownership changes that the
  Android app UID cannot represent are accepted after `EPERM`/`EACCES`; files
  stay owned by the sandbox UID. Mode changes still execute normally.

The fixed trixie rootfs uses Perl 5.40, so the launcher supplies rooted
`PERL5LIB` entries for Debian debconf modules. The package cohort exercised a
real `libc-bin` trigger and `ldconfig -r` in addition to shell and Perl
maintainer scripts.

## Acceptance evidence

`hello` installed as version `2.10-5` for `arm64`; `dpkg -V hello` reported no
package-file differences, and execution through the rootfs loader printed:

```text
Hello, world!
```

Removal made `dpkg -s hello` return 1, and reinstall succeeded from apt's
cache. For the final acceptance, all seven newly introduced X11 packages were
removed and `tools/bxapt install x11-apps` was rerun once from a clean package
state. It exited zero, as did `apt check`. `x11-apps` installed as
`7.7+11+b2`; `man-db`, its shell/Perl maintainer path and both `libc-bin`
triggers all configured in that single run. The unmodified package
`/usr/bin/xclock` then rendered at 1920x1080 through the embedded X11 server;
screenshots three seconds apart differed in the seconds glyph. The screenshot
is `evidence/bxapt-xclock.png`.
