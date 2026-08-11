# Detached glibc X11 session supervision

## Boundary exposed by WPS

Writer's `xdg-open` workflow uses Qt's detached-process path. Before this
change, the resulting `wpspdf` was reparented to PID 1 while
`bionicx-exec` continued to wait only for Writer. Destroying the Activity could
therefore stop the X server without owning all of its clients, and Chrome's
zygote/renderer tree would have the same structural problem.

## Controlled integration client

`session-x11-probe` is a genuine AArch64 glibc/libX11 program. It double-forks,
calls `setsid`, lets its primary process exit, and only then has the detached
grandchild connect and draw a window. This deliberately escapes the primary
process group and rules out a test that passes merely because the initial PID
stayed alive.

On x300, the primary exited at 05:41:49.874. The detached window remained
visible and responsive for 15 seconds, then the client reported all three
assertions and exited. The executor reaped one adopted child and drained the
session only at 05:42:05.498. The rendered result is
`evidence/session-x11-supervised.png` and the compact log is
`evidence/session-x11-supervision.log`.

## Executor behavior

`bionicx-exec` now becomes a Linux child subreaper before launching the target.
It restores default termination signals and an empty signal mask before the
glibc handoff, places the primary in its own process group, and waits for every
adopted descendant after the primary exits. The short ptrace bootstrap still
detaches at loader entry; session supervision uses ordinary process APIs.

Android's kernel on this device does not expose
`/proc/<pid>/task/<tid>/children`, and the app sandbox does not provide a
writable per-session cgroup. On shutdown the executor therefore signals both
the primary group and every process it is permitted to signal. BionicX assigns
one Android UID to one active display session, so this is an explicit UID-level
session boundary rather than an unscoped system operation.

The shutdown regression sent SIGTERM to the executor as the ordinary app UID
while the grandchild was in a distinct session. Both supervisor and worker
were gone within three seconds; the log records `stopping session signal=15`
and exit 143. No root, Frida, PRoot, Termux, or long-lived ptrace was involved.
The unchanged runtime probe subsequently passed 20/20 and drained zero extra
children.

