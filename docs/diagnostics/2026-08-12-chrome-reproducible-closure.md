# Reproducible Chrome and runtime-module closure

## Gap

The first successful Chrome navigation depended on libraries copied manually
after each failure. Recursive `DT_NEEDED` traversal correctly found linked
dependencies but could not infer NSS modules loaded by name at runtime. A fresh
device would therefore fail at `libsoftokn3.so` even though the POC device ran.

## Bundle design

`examples/chrome/build-bundle.sh` pins Google Chrome stable ARM64
151.0.7922.108-1 and package SHA-256
`23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499`.
A native ARM64 Debian container resolves packages so host amd64 state cannot
satisfy dependencies accidentally. All 155 inputs are checked against
`dependencies.lock`; package drift stops the build.

The resolver accepts multiple entries and now supports `--exclude-copy-root`.
It can inspect Chrome's installed executables without duplicating the 282 MB
main ELF into the flat library directory. Five declared runtime roots cover
NSS softoken, freebl, builtin roots and DBM modules. Their `.chk` companions
are copied as data. The resulting report contains 89 ELF objects, an empty
`missing` map, 38 MB of closure libraries and a relative-path hash manifest.

The compact glibc/X11 runtime also carries glibc's post-2.34 compatibility DSOs
(`libdl`, `libpthread`, `libresolv`, `librt`, and `libutil`) because legacy
desktop ELFs still contain those SONAMEs even when their implementations live
in libc.

## Device verification

The generated app tree, rather than the previous hand-built tree, was installed
on x300. A cold untraced launch under the ordinary app UID navigated with
Android-injected input and rendered Example Domain at 1920x1080. The log has
zero fatal/NSS/FD-ownership/network-service failures and zero XFixes errors.
Remaining messages are the separately tracked D-Bus and machine-id service
gaps.

See `evidence/chrome-repro-bundle.log`,
`evidence/chrome-repro-bundle.png`, and
`evidence/chrome-repro-bundle.txt`.
