# XTerm: terminal integration application

This example runs Debian trixie's real ARM64 `xterm` from the same pinned,
apt/dpkg-installed rootfs as Chrome, WPS and IceWM. It is the terminal category
integration target, not a synthetic painted-window probe.

The profile starts an interactive Debian bash inside xterm. BionicX adapts the
shared `/usr/bin/bash` interpreter to the app-private glibc loader because
there is intentionally no chroot or PRoot to resolve Debian's stock absolute
loader path. The inherited multiarch `LD_LIBRARY_PATH` remains package based.

Build and install with:

```sh
examples/xterm/build-bundle.sh
tools/install-profile.sh --profile profiles/xterm.json \
  --app-root build/xterm-bundle/app \
  --runtime-root build/xterm-bundle/rootfs
```

Acceptance requires more than a mapped terminal window: inject a command into
the terminal and retain evidence that bash echoed the command and rendered its
output. The final launch must be untraced under the ordinary Android app UID.
