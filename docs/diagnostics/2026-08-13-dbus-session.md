# App-private Debian D-Bus session

## Problem

The shared Debian rootfs already contained `dbus-daemon`, `dbus-send`, its
session policy and activation metadata, but BionicX did not start a bus.
FileZilla consequently logged `Could not parse server address` when its profile
used the earlier `disabled:` placeholder. Thunderbird also had to fall back
around the missing desktop session.

Copying more libraries cannot solve this: D-Bus is a live IPC service. It also
does not require PRoot, a complete init system or a Bionic reimplementation.

## Implementation

`hostServices: ["dbus"]` now adds a lifecycle component before the glibc client
starts. The component runs the unmodified trixie ARM64 `dbus-daemon` from the
shared rootfs with its packaged `session.conf`, in foreground mode, as the
ordinary APK UID. Its socket is app-private at `${TMP}/runtime/bus`, and the
launcher supplies the same address through `DBUS_SESSION_BUS_ADDRESS`.

Readiness is the existence of the listening socket. Normal Activity teardown
terminates the daemon and removes the socket. Android force-stop kills the
process group before Java teardown can run, so the next launch deliberately
deletes the stale socket before binding it again.

## Controlled result on x300

`profiles/dbus-probe.json` launches the package-installed glibc `dbus-send` and
calls `org.freedesktop.DBus.ListNames`. It returned the bus name and its own
unique name, exited zero, and repeated the same result after a force-stop and
restart. The raw two-cycle transcript is retained in
`evidence/dbus-session-restart.log`.

The final canonical rootfs makes `dbus=1.16.2-2` an explicit apt root rather
than relying on an application's transitive dependency. Its content ID is
`84e4d9c00d4049517093ed2217b48bea0014c7ec9d44114c0c15b58d377cff38`;
the probe and FileZilla bus connection were repeated after that exact image was
installed on x300.

## Real applications

FileZilla 3.68.1 rendered its complete main window without its previous invalid
address diagnostic. `ListNames` showed two persistent unique connections;
`GetConnectionUnixProcessID` mapped both to the actual FileZilla process.

Thunderbird 140.13 ESR rendered account setup and acquired a persistent
`org.mozilla.thunderbird.*` application name. Its only remaining startup lines
in the captured BionicX log concern GPU probing and unavailable Android video
devices, not D-Bus.

The screenshots and exact bus queries are in `evidence/`. This proves session
transport and real client name ownership. It does not yet certify optional
activated services such as portals, notifications, secrets or accessibility;
those remain separate capabilities to add only when an application exercises
them.
