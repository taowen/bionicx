# VLC AVI fixture probe

Untraced VLC on the shared seed must demux the deterministic
`bionicx-motion-audio.avi` (90 I420 frames + 48 kHz stereo PCM), not just
reach a splash screen. This probe loads Debian `libvlc.so.5` with dummy
input/output and requires the media to enter Playing, report `320x180`, and
last about three seconds.

```sh
ANDROID_SERIAL=<serial> examples/vlc-avi-probe/install-and-run.sh
```

Expect `BXSUMMARY vlc-avi passed=10 failed=0`. The probe must not replace the
shared seed.
