# Android DNS bridge and Chrome navigation

## Symptom

Chrome reached a complete, readable error page but every navigation ended in
`DNS_PROBE_FINISHED_BAD_CONFIG`. Android publishes DNS servers through the
active network's `LinkProperties`; it does not create the `/etc/resolv.conf`
that a Debian glibc process expects.

## Controlled correction

Profiles that opt into `android-dns` now receive the active network's IPv4 DNS
servers from the Android host. `libbionicx-android-dns.so` preserves glibc's
resolver initialization and replaces its empty IPv4 nameserver list. An
explicit profile `BIONICX_DNS_SERVERS` remains authoritative, which keeps the
bridge deterministic and independently testable.

`network-x11-probe` is a genuine AArch64 glibc client linked to libresolv and
libX11. On x300 under the ordinary app UID, with no root, ptrace, Frida, proot,
or Termux, it passed all five boundaries:

```text
BXTEST PASS resolver-init glibc-res_state-ready
BXTEST PASS android-dns-config nscount=1 first=192.168.1.1
BXTEST PASS dns-a-query answers=2 address=104.20.23.154
BXTEST PASS tcp-http bytes=865 status=200 body=matched
BXTEST PASS x11-result-window expose=yes
BXTEST SUMMARY pass=5 fail=0
```

The returned address is evidence from this run, not a pinned expectation. See
`evidence/network-x11-probe.log` and `evidence/network-x11-probe.png`.

## Chrome result and the dependency-closure gap

The first post-DNS Chrome run advanced into TLS but failed because
`libsoftokn3.so` was absent. It is loaded by NSS at runtime rather than exposed
as a main-executable `DT_NEEDED` edge. Installing the matching package modules
(`libsoftokn3`, `libfreebl3`, `libfreeblpriv3`, `libnssckbi`, `libnssdbm3`, and
SQLite) removed the NSS failure. The generic dependency resolver already
accepts repeated `--entry` roots, so the Chrome installer must declare these
runtime module roots when it is made reproducible; this is intentionally still
listed as pending rather than hidden by the successful device state.

Chrome stable ARM64 151.0.7922.108-1 then loaded Example Domain over real DNS,
TCP, and TLS and rendered it through the embedded X server. The 20-second log
contains no NSS failure, fatal error, child FD-ownership violation, or network
service restart loop. Remaining messages expose separate desktop-service gaps
(D-Bus and `/etc/machine-id`). The black right side of the screenshot is the
independent X11 root-window sizing/fullscreen issue.

See `evidence/chrome-network.log` and `evidence/chrome-network-ui.png`.
