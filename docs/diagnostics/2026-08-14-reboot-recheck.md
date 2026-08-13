# Force-stop and reboot recheck

On `01408BH601027129`, after `am force-stop io.taowen.bx` and a full
`adb reboot`, the seed id is still
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.

Host probes (runtime contract, new-device guide, ELF fixup, VLC AVI,
soffice-doc, popular profiles) still print PASS. On the device,
`vlc-avi-probe` is 10/10 and `soffice-origin-probe` is 5/5 with
`LibreOffice 25.2.3.2`. Untraced `hello-x11` logs `glibc=2.41`.

`glx-probe` after wake is 25/26: `glx-present` and the compositor
blue/red pixels pass; `glx-fbconfig-visual` reports `visual=0x0
xRenderable=0` on the first two cold starts. That one GLX config query
is recorded as remaining, not as a seed wipe.

`docs/NEW-DEVICE.md` still names seed rebuild, `bxapt` declarations,
and the Android-kernel limits (`set_robust_list`, `clone3`,
`--no-sandbox`).
