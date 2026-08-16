# XFCE desktop session

Starts package-installed `xfwm4 --compositor=off`,
`xfce4-panel` and `xfdesktop`, then Thunar and Mousepad from the shared
rootfs. The compositor stays off until an `xfwm4 --compositor=on`
session is green. After the windows map, `--accept` checks that xfwm4 advertised
`_NET_SUPPORTING_WM_CHECK`, the panel is mapped, focus can switch,
Thunar can resize (shrink when the saved window already fills
the screen), and Mousepad can close and reopen. D-Bus,
PulseAudio, CUPS and Vulkan are requested through `hostServices`. There
is no `xfce4-session` / systemd login manager; the components are
started directly. `xfsettingsd` is not started yet.

```sh
ANDROID_SERIAL=<serial> examples/xfce-session/install-and-run.sh
```
