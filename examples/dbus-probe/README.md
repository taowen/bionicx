# D-Bus session integration probe

This profile asks BionicX to supervise Debian trixie's unmodified
`dbus-daemon`, then runs the package-installed `dbus-send` client against the
app-private session socket. A pass returns the bus daemon's unique name and
`org.freedesktop.DBus` from `ListNames`, and the daemon is stopped with the
Android activity.

```sh
examples/dbus-probe/build-bundle.sh
tools/install-profile.sh --profile profiles/dbus-probe.json \
  --app-root build/dbus-probe-bundle/app \
  --runtime-root build/dbus-probe-bundle/rootfs
```
