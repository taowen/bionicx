# D-Bus from the clean identity-fixed seed

Device `01408BH601027129` kept seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
`bxapt install dbus` unpacked in one wave (`remaining=0`) and configured
`dbus` 1.16.2-2 plus `dbus-bin`, `dbus-daemon`, and the session/system bus
common packages. `--no-install-recommends` left `dbus-user-session` (and
therefore full `systemd`) uninstalled. `messagebus` is uid/gid 999.

`dpkg --audit` is empty. There is no `libc.so.6` under `files/apps`.

Untraced `profiles/dbus-probe.json` started the shared session bus, and
`dbus-send --session ListNames` returned `org.freedesktop.DBus` and `:1.0`
before exiting 0.
