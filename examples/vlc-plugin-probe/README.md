# VLC plugin loader probe

Debian `libvlc.so.5` plus `libxcb_x11_plugin.so` on the shared seed.
Creates a dummy libvlc instance. Does not demux an AVI or start the VLC GUI.
Playback of `bionicx-motion-audio.avi` belongs to the `vlc` app profile.

```sh
ANDROID_SERIAL=<serial> examples/vlc-plugin-probe/install-and-run.sh
```

Expect `BXSUMMARY vlc-plugin passed=4 failed=0`. The probe must not replace
the shared seed.
