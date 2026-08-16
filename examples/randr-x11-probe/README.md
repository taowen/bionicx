# RandR write probe

libXrandr only. Two clients cover the display-settings contract:
screen size range, primary output, CRTC config, gamma get/set,
`SetScreenSize`, `GetMonitors`, and the GTK RandR 1.5 follow-up
(`GetOutputInfo`, EDID `GetOutputProperty`, `ListOutputProperties`,
monitor name atom), including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/randr-x11-probe/install-and-run.sh
```
