# Force-stop and reboot recheck

Runs host contract tests, then `am force-stop` and `adb reboot` on the
device. After `run-as` is back, checks the seed id, empty `dpkg --audit`,
durable qBittorrent/Krita files, `keepassxc-cli-probe` 6/6, records
`glx-probe` (CreateContext may fail on a cold boot), and the untraced
IceWM two-app desktop session 7/7.

Do not pass a runtime-root replacement.

```sh
ANDROID_SERIAL=<serial> examples/reboot-recheck/run.sh
```
