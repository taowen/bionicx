# Shared IceWM desktop session

Starts package-installed IceWM plus two unrelated Debian applications
(`xterm` and `mousepad`) under one profile. D-Bus and CUPS are requested
through `hostServices`. PulseAudio and Vulkan stay on the later audio/GPU
gates. The session binary is the only app-private payload; libraries stay
in the shared rootfs.

```sh
ANDROID_SERIAL=<serial> examples/desktop-session/install-and-run.sh
```
