# V2509A OriginOS APK install without a manual tap

Device: `10AFA31610002QH` (vivo V2509A / PD2509, Android 16 / API 36,
Mali, `PAGE_SIZE` 4096). `wm size` is `1216x2640`. There is no `su`.

`adb install` waits on the OriginOS package-installer risk page. The same
coordinates used by arctrl work here:

1. Risk checkbox `(607, 2289)`
2. Continue `(607, 2462)`

`tools/install-apk.sh` detected V2509A, pushed `build/bionicx-debug.apk` to
`/data/local/tmp`, ran `pm install -r -t` under a tap loop, and printed
`Success` with no host-side approval.

It then staged `files/bin/bionicx-exec` and `files/lib/libbionicx-runtime.so`
from the APK so `bxapt normalize` does not wait for the first Activity.

`ANDROID_SERIAL=10AFA31610002QH examples/hello/install-and-run.sh` installed
the shared seed (`ed998c095a1b9384b1f022d06101ac3fc3c61761ac751546bd84edca298e44e2`)
and launched hello. Logcat:

```
bionicx-exec: running untraced pid=18593
hello-x11: glibc=2.41 pid=18593 DISPLAY=:0 vendor=Elbrus Technologies, LLC
```

The process is ordinary app UID `u0_a381` (`uid=10381`), `targetSdk=28`,
`which su` is empty, `ro.secure=1`. Screenshot:
`evidence/vivo-10AFA31610002QH/hello.png` (900x480 hello card on the
2640x1216 X screen).
