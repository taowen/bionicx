# Next declared slice from packages/trixie-popular.txt

After cups, `bxapt install thunar geany bsdextrautils` on seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`
unpacked in three Pre-Depends waves (`remaining=2`, then `1`, then `0`)
and configured all three requested packages.

`systemd` was pulled automatically and `systemd-standalone-sysusers` was
removed. `dpkg --audit` is empty. Manual marks include `thunar`, `geany`
and `bsdextrautils`; `systemd` is auto. No `libc.so.6` under `files/apps`.

The rest of `packages/trixie-popular.txt` (Firefox, LibreOffice, GIMP,
Krita, WPS/Chrome debs, …) is still not installed.
