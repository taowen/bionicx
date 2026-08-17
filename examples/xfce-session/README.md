# XFCE desktop session

Starts package-installed `xfwm4 --compositor=on --vblank=off`,
`xfsettingsd --disable-wm-check`, `xfce4-panel`, Thunar (so it owns
`org.xfce.Thunar` before xfdesktop D-Bus-activates FileManager),
`xfdesktop`, then Mousepad from the shared rootfs. The session home pins
`Net/IconThemeName=Adwaita` because the seed's `Tango` directory is a
Geany stub without `index.theme`, and sets `XDG_MENU_PREFIX=xfce-` so
garcon loads `xfce-applications.menu`. After the windows map,
`--accept` checks that xfwm4 advertised `_NET_WM_CM_S0`, xfsettingsd
owns `_XSETTINGS_S0`, `_NET_SUPPORTING_WM_CHECK`, the panel is mapped, focus can switch,
Thunar can resize (shrink when the saved window already fills
the screen), Mousepad can close and reopen, and `xfce4-popup-applicationsmenu` maps
the Applications menu. D-Bus,
PulseAudio, CUPS and Vulkan are requested through `hostServices`. There
is no `xfce4-session` / systemd login manager; the components are
started directly.

```sh
ANDROID_SERIAL=<serial> examples/xfce-session/install-and-run.sh
```
