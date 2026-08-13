# Reconstruct from the pinned seed plus bxapt declarations

Device `01408BH601027129` still carries identity-fixed seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
The shared rootfs was not wiped. `tools/bxapt set packages/trixie-popular.txt`
installed the one missing declaration (`qt6-qpa-plugins 6.8.2+dfsg-9+deb13u2`)
and marked every declared package manual.

Hash-pinned Chrome `151.0.7922.108-1` and WPS `11.1.0.11720` stay `ii`; the
cached debs match `packages/external-arm64.tsv`. `cups-daemon` and
`cups-client` stay `ii`. `dpkg --audit` is empty before and after
`--reinstall bsdextrautils`, after `remove ristretto`, and after `set`
restores ristretto. `find files/apps -name libc.so.6` is empty.

Evidence: `evidence/rebuild-2026-08-13/bxapt-reconstruct.log`.
