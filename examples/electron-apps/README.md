# Electron app profiles

Installs Debian VS Code or Feishu onto the shared seed and seeds a
workspace plus a tiny unpacked extension that opens an editor-tab
terminal. Does not replace the device rootfs.

```sh
ANDROID_SERIAL=<serial> examples/electron-apps/install-and-run.sh
BIONICX_ELECTRON_PROFILE=profiles/feishu.json \
ANDROID_SERIAL=<serial> examples/electron-apps/install-and-run.sh
```

The editor-tab terminal is a `bash -c` read/eval loop. The unpacked
extension is seeded into both `~/.vscode/extensions` (what this build
loads) and `profile-vscode/extensions`.
