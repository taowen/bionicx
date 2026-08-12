# glibc X11 + Android audio probe

This controlled ARM64 glibc client opens a real X11 window and uses Debian's
`libpulse-simple.so.0` to play five seconds of deterministic 440 Hz stereo PCM.
The profile-selected Bionic PulseAudio server forwards the stream to Android's
AAudio output.

```sh
examples/audio-probe/build.sh
tools/install-profile.sh --profile profiles/audio-probe.json \
  --app-root build/audio-probe-bundle/app \
  --runtime-root build/popular-apps-bundle/rootfs
```

Acceptance requires the client's `BXSUMMARY`, a visible green playing window,
and a concurrent AudioFlinger active track with zero write errors and underruns.
Client success alone is not sufficient.
