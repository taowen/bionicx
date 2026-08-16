# Durable Krita, qBittorrent and KeePassXC workflows

Runs the existing probes first, then the untraced GUI profiles on the
shared seed:

- KeePassXC: `examples/keepassxc/seed-db.sh` (create/add/ls/show/reopen/persist),
  then the deferred-open GUI and a CLI `show` of the same `.kdbx`.
  The DatabaseWidget NULL `d_ptr` repro is `keepassxc-db-widget-probe`.
- qBittorrent: hash-pinned 256 KiB payload plus `.fastresume`, GUI,
  force-stop persist, cold relaunch.
- Krita: `krita-glx-destroy-probe` (`glXDestroyContext(NULL)`), GUI of
  the 640×480 PPM, then `krita --export` on the live X display to
  `Documents/bionicx-saved.png`.

Do not pass `--runtime-root`. The fixture bundles must not replace the
device seed.

```sh
ANDROID_SERIAL=<serial> examples/popular-workflows/run.sh
```
