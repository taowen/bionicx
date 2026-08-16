# Electron app profiles

Installs Debian VS Code or Feishu onto the shared seed and seeds a
workspace plus a tiny unpacked extension that opens an editor-tab
terminal. Does not replace the device rootfs.

```sh
ANDROID_SERIAL=<serial> examples/electron-apps/install-and-run.sh
BIONICX_ELECTRON_PROFILE=profiles/feishu.json \
ANDROID_SERIAL=<serial> examples/electron-apps/install-and-run.sh
```

The VS Code terminal uses `bash -c` with a read/eval loop because
`bash -i` still exits 0 under node-pty. That remaining failure belongs
to a forkpty/`login_tty` probe, not to clicking around the IDE.
