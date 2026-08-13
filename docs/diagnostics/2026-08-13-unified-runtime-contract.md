# Unified Debian runtime contract

## Decision

Per-profile compatibility modules were producing different Linux personalities
for D-Bus, apt and applications. Schema version 2 removes the `compatibility`
field. Every Debian process now receives exactly one preload:
`libbionicx-runtime.so`. The runtime owns Android seccomp behavior, FHS path
mapping, app-UID identity and DNS. Profiles cannot override its reserved
environment variables.

The old WPS and Chrome preload libraries are deleted rather than retained as
aliases or fallbacks. D-Bus and `bxapt` use the same executor, loader, shared
rootfs and runtime library as GUI applications.

## Android kernel boundary

Debian glibc 2.41 attempts `clone3` while `pthread_create` has all ordinary
signals blocked. Android reports a forbidden syscall as synchronous SIGSYS, so
a signal handler cannot return `ENOSYS` on this path. The runtime validates the
exact AArch64 glibc syscall stub before disabling that unavailable syscall;
glibc then uses its normal `clone` implementation. Qt additionally probes SysV
SHM and the legacy `accept` syscall; the runtime exposes the consistent Android
result and uses `accept4`, which Android permits.

Android also denies `set_robust_list`. Silently claiming robust owner-death
semantics caused a mutex to wait forever, so the public robust-mutex attribute
now returns `ENOTSUP`. The runtime probe reports this as a capability, not a
passing implementation. Ordinary pthread create/join remains mandatory.

## x300 acceptance (01408BH601027129)

- Runtime probe: 19 required checks passed, 0 failed. `pthread-create-join`,
  TCP loopback and a real X11 window passed; robust pthread mutexes, SysV SHM,
  POSIX SHM and user namespaces were reported as unavailable capabilities.
- D-Bus probe: Debian `dbus-send` completed `org.freedesktop.DBus.ListNames`,
  returning `org.freedesktop.DBus` and its client name with exit status 0.
- qBittorrent: Debian trixie Qt6 application launched without signal diagnosis,
  displayed its real main window and retained the deterministic 256 KiB
  web-seed transfer at 100%/Seeding.
- Package state: the unified `bxapt query` reported `dbus`, `qbittorrent` and
  `google-chrome-stable` as `ii`; `bxapt apt check` exited 0.

The deterministic downloaded payload and source both have SHA-256
`a68590ec9ed4b1530a44cfb5f9df3457503ebc106d1b0124d865ca217f38537d`.
