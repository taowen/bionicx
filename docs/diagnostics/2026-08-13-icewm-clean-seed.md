# IceWM 3.7.4 from the clean identity-fixed seed

Device `01408BH601027129` kept seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
`bxapt install icewm` unpacked in one wave (`remaining=0`) and configured
`icewm` 3.7.4-1 and `icewm-common`.

`dpkg --audit` is empty. There is no `libc.so.6` under `files/apps`.

The existing `icewm-probe` was installed with `--app-root` only so the seed
was not replaced. Untraced it reports `BXSUMMARY icewm passed=4 failed=0`
(manager start, taskbar, decoration pixels, two managed clients). The
screenshot shows both frames and the IceWM taskbar.
