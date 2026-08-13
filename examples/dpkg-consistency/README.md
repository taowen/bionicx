# Declared-package dpkg consistency

Checks every name in `packages/trixie-popular.txt`,
`packages/external-arm64.tsv`, plus `cups-daemon`/`cups-client`:

- `dpkg --audit` empty
- each package `install ok installed`
- no `libc.so.6` / loader / `libstdc++.so.6` under `files/apps`
- `bxapt install --reinstall bsdextrautils`
- `bxapt remove ristretto` then `bxapt set packages/trixie-popular.txt`

Do not pass a runtime-root replacement. The shared seed stays in place.

```sh
ANDROID_SERIAL=<serial> examples/dpkg-consistency/run.sh
```
