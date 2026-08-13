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

Device after reboot:

- `keepassxc-cli-probe` 6/6
- IceWM desktop session `--accept` 7/7 (xterm+mousepad, D-Bus, Pulse,
  CUPS, Vulkan host service)
- `glx-probe` `glx-fbconfig-visual` still `configs=3 visual=0x1
  xRenderable=1`, then `glXCreateContext` returns NULL
  (`passed=9 failed=1`). `krita-glx-destroy-probe` is 2/2 on
  `create-new`. Waiting for `MapNotify` does not change that.

The reboot matrix therefore covers seed, audit, durable files, the
KeePassXC CLI path, and the untraced two-app desktop session.
Gladio window-context creation after a cold boot is still a remaining
GPU gap, not treated as 26/26.

Evidence: `evidence/rebuild-2026-08-14/reboot-recheck.log`,
`keepassxc-cli-probe-reboot.log`, `desktop-session-reboot.log`,
`glx-probe-reboot.log`, `krita-glx-destroy-reboot.log`.
