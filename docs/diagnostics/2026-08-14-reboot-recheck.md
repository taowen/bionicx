# Force-stop and reboot recheck

On `01408BH601027129`, after `am force-stop io.taowen.bx` and `adb reboot`,
the seed is still
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
`run-as` needs a few extra seconds before `bxapt` push works
(`packagelist_parse` Permission denied immediately after boot).

Durable files survive: qBittorrent payload `a68590ec…`, Krita
`bionicx-saved.png`. `dpkg --audit` is empty once run-as settles.

Host probes (runtime contract, new-device guide, GLX seed-safe,
krita-glx-destroy, keepassxc-cli, popular-durable, dpkg-consistency,
firefox-online) still print PASS.

Device after reboot (seed unchanged, audit empty, qBit/Krita files
intact):

- `keepassxc-cli-probe` 6/6
- `glx-probe` `BXSUMMARY host-glx passed=26 failed=0` (CreateContext
  no longer NULL; unsared GLES3 fallback if the keyguard hid the
  compositor surface). `screencap` can truncate if adb drops.
- IceWM `--accept` is 7/7 once the keyguard is dismissed (`wm
  dismiss-keyguard` + swipe): xterm+mousepad map, switch, resize
  644x340→740x382, close, reopen (`desktop-session-accept.log`). A
  locked start leaves `isSleeping=true` and stalls `XOpenDisplay`.

Rerun `examples/reboot-recheck/run.sh` to recapture under `build/evidence/`.
