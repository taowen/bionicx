# X11 window-tree observer

`x11-window-tree` is a small, genuine AArch64 glibc/Xlib client for observing
another client's top-level and child windows. It recursively uses `QueryTree`,
reads `_NET_WM_NAME`, `WM_NAME`, and `WM_CLASS`, and reports map state,
override-redirect state, and geometry.

It is deliberately separate from an application profile: copy the built binary
into an already installed app tree and execute it with that profile's glibc
loader while the embedded X server is running. This makes hidden, unmapped, and
off-screen native dialogs observable without a debugger or client injection.

```sh
examples/x11-window-tree/build.sh
```
