# cups-daemon and cups-client on the clean identity-fixed seed

Device `01408BH601027129` kept seed
`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`.
`bxapt install cups-daemon cups-client` unpacked in one wave (`remaining=0`).

The first configure failed: `addgroup --system lpadmin` reported
`/etc/group: Read-only file system`. Launching the installed APK had
replaced `files/lib/libbionicx-runtime.so` (77376 bytes) with a build
that predates the fortified `open`/`lckpwdf` hooks. Restoring the
current runtime (78320 bytes, exports `__open_2` and `lckpwdf`) and
running `bxapt dpkg --configure -a` configured:

```text
cups-client 2.4.10-3+deb13u2 ii
cups-daemon 2.4.10-3+deb13u2 ii
ssl-cert    1.1.3            ii
```

`getent` shows `lpadmin` (101), `ssl-cert` (102) and `lp` (7).
`dpkg --audit` is empty. `cupsd -t` exits 0. There is no `libc.so.6`
under `files/apps`.

`bxapt` now refreshes `libbionicx-runtime.so` from the repository assets
on every prepare so a stale APK extract cannot break the next transaction.
