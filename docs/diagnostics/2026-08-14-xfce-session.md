# XFCE desktop session with two package apps

Device `10AFA31610002QH` (vivo V2509A, Mali-G1-Ultra).

`profiles/xfce-session.json` requests `dbus`, `pulseaudio`, `cups` and
`vulkan`. The launcher starts package-installed `xfwm4 --compositor=off`,
`xfce4-panel`, `xfdesktop`, Thunar and Mousepad from the shared rootfs.
There is no `xfce4-session` / systemd login manager.

xfwm4's default compositor dies on unimplemented Composite
`RedirectSubwindows` (and would need XDamage). The session therefore
turns the compositor off. The server now accepts `RedirectSubwindows` /
`UnredirectSubwindows` without taking children offscreen, maps
`GrabServer` owners and `_NET_WM_WINDOW_TYPE_DOCK` windows even while a
WM holds `SubstructureRedirect`, and accepts `GrabKey` pointer-mode 0.

On vivo:

```text
BXTEST PASS xfce-session-launch children=5
BXTEST PASS session-display :0
BXTEST PASS xfce-wm xfwm4
BXTEST PASS xfce-panel mapped
BXTEST PASS session-two-mapped thunar=0x1c00009 mousepad=0x2000003
BXTEST PASS session-switch-thunar focus=thunar
BXTEST PASS session-switch-mousepad focus=mousepad
BXTEST PASS session-resize-thunar 1408x864 -> 1504x912
BXTEST PASS session-close-mousepad client withdrawn
BXTEST PASS session-reopen-mousepad pid=25807 alive=1
BXSUMMARY xfce-session-accept passed=9 failed=0
```

No per-app `libc.so.6`. Evidence: `evidence/vivo-10AFA31610002QH/xfce-session.png`.

```sh
ANDROID_SERIAL=<serial> examples/xfce-session/install-and-run.sh
```
