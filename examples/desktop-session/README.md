# Shared IceWM desktop session

Starts package-installed IceWM plus two unrelated Debian applications
(`xterm` and `mousepad`) under one profile. After they map, `--accept`
switches focus, resizes xterm, sends `WM_DELETE_WINDOW` to mousepad and
reopens it. D-Bus, PulseAudio, CUPS and Vulkan are requested through
`hostServices`. The session binary is the only app-private payload;
libraries stay in the shared rootfs.

```sh
ANDROID_SERIAL=<serial> examples/desktop-session/install-and-run.sh
```
