# RandR write probe

libXrandr only. One client covers the display-settings contract:
screen size range, primary output, CRTC config, gamma get/set, and
`SetScreenSize`, including under `GrabServer`.

```sh
ANDROID_SERIAL=<serial> examples/randr-x11-probe/install-and-run.sh
```
