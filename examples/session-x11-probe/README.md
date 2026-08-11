# Detached glibc X11 session probe

This genuine AArch64 glibc/libX11 client double-forks, creates a new session,
and lets its primary process exit before the detached grandchild connects to
X11. It proves that `bionicx-exec` adopts and supervises GUI descendants rather
than equating the initial process with the complete desktop session.

Run on Android with:

```sh
ANDROID_SERIAL=01408BH601027129 \
  examples/session-x11-probe/install-and-run.sh
```

Success requires all three client assertions, the executor's primary-exit
marker, and a drained session after the detached window exits.
