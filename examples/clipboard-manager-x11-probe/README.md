# Clipboard manager persist probe

libX11 plus XFixes. Two connections follow the xfsettingsd clipboard
daemon path without starting it: one client owns `CLIPBOARD_MANAGER`,
watches `CLIPBOARD` with XFixes, caches `UTF8_STRING`, takes ownership
after the peer owner is destroyed, and still serves the bytes.

```sh
ANDROID_SERIAL=<serial> examples/clipboard-manager-x11-probe/install-and-run.sh
```
