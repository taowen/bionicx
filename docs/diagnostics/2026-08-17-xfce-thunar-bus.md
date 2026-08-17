# Thunar lost `org.xfce.Thunar` to xfdesktop

The session started `xfdesktop` before Thunar. xfdesktop D-Bus-activates
`org.xfce.FileManager` / `org.freedesktop.FileManager1`, which launches
a Thunar that takes `org.xfce.Thunar`. The explicit Thunar then logged:

```text
Failed to register: Unable to acquire bus name 'org.xfce.Thunar'
```

Desktop icon open and FileManager calls need that name.

Start Thunar first. `examples/xfce-session/install-and-run.sh` fails if
the race returns. vivo `xfce-session-accept` stays 11/11 without the
error.
