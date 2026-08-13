# bxapt interrupted transaction recovery

`bxapt` now persists transaction metadata under the app-private
`files/run/bxapt` directory:

- `installed-packages-before.txt`: the successful `ii` package set before the
  transaction;
- `requested-packages.txt`: packages explicitly requested by the caller;
- `unpacked-paths.txt`: paths from the manually unpacked archive set.

The new `bxapt recover` command requires all three files. It incrementally
normalizes the retained unpack manifest, runs `apt-get -f install` to finish
unpacked or half-configured packages, reconciles apt manual/automatic marks
against the retained snapshot, then removes the metadata. Normal successful
install/set transactions also remove the metadata, so a completed transaction
cannot be replayed as a recovery.

Host and device checks completed on 2026-08-13:

```text
bxapt recovery metadata: PASS
runtime contract probe: PASS
rootfs ELF fixup: PASS
```

The latest APK was installed on `01408BH601027129`. A controlled pending
transaction was then staged in the app-private transaction directory with an
empty unpack manifest and the retained `dbus` snapshot. The real device
`bxapt recover` completed successfully as the ordinary app UID:

```text
bionicx ELF fixups: 1277 entries
0 upgraded, 0 newly installed, 0 to remove and 0 not upgraded.
... apt-mark reconciliation ...
exit=0
```

The recovery metadata was removed after completion. The package database
remained configured and no root, PRoot, Termux or Frida process was used.
