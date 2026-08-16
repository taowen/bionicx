# XFCE desktop session

Starts package-installed `xfwm4 --compositor=on --vblank=off`,
`xfsettingsd --disable-wm-check`, `xfce4-panel` and `xfdesktop`, then
Thunar and Mousepad from the shared rootfs. After the windows map,
`--accept` checks that xfwm4 advertised `_NET_WM_CM_S0`, xfsettingsd
owns `_XSETTINGS_S0`, `_NET_SUPPORTING_WM_CHECK`, the panel is mapped, focus can switch,
Thunar can resize (shrink when the saved window already fills
the screen), and Mousepad can close and reopen. D-Bus,
PulseAudio, CUPS and Vulkan are requested through `hostServices`. There
is no `xfce4-session` / systemd login manager; the components are
started directly.

```sh
ANDROID_SERIAL=<serial> examples/xfce-session/install-and-run.sh
```
