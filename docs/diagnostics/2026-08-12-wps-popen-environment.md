# Sanitize glibc loader variables before Android shell execution

## Failure

The WPS compatibility module routes glibc `popen` to `/system/bin/sh` because
there is no Linux `/bin/sh` in the app bundle. It already removed `LD_PRELOAD`,
but the child retained WPS's glibc `LD_LIBRARY_PATH`. Android's Bionic linker
therefore found the rootfs `libc.so` linker script and rejected it as an ELF:

```text
CANNOT LINK EXECUTABLE "sh": ".../rootfs/usr/lib/libc.so" has bad ELF magic
```

## Controlled regression

`profiles/wps-compat-probe.json` reuses the genuine glibc runtime probe, loads
the WPS compatibility module, deliberately exports the rootfs
`LD_LIBRARY_PATH`, and calls `popen("printf bionicx-android-shell")`. The forked
child now removes both `LD_PRELOAD` and `LD_LIBRARY_PATH` before executing the
Bionic shell.

On x300:

```text
BXTEST PASS android-shell-popen bytes=21 status=0 output=bionicx-android-shell
BXSUMMARY runtime passed=21 failed=0
```

The complete controlled run is in `evidence/wps-compat-probe.log`.

## Real WPS verification

WPS subsequently ran both observed commands through the same path:

```text
popen via Android shell: gsettings get org.ukui.SettingsDaemon.plugins.tablet-mode tablet-mode 2>/dev/null
popen via Android shell: pidof wpsupdate
```

`evidence/wps-shell-env-fixed.log` contains neither `CANNOT LINK EXECUTABLE` nor
an abnormal WPS exit. RandR primary-output and XKB warnings are independent
remaining server gaps.
