# Bring BionicX up on a new Android device

This is the reproducible path for an ARM64 Android 14 device with 4 KiB
pages. The test serial used while writing it is `01408BH601027129`
(AYANEO Pocket FIT, API 34).

## What you need

Linux host, Android SDK/NDK, JDK 17, Podman, Python 3, patchelf, ADB.
The device must allow `adb` as an ordinary app UID. Root is not required
to run applications.

## Build

```sh
cd bionicx
git submodule update --init
tools/build.sh
tests/test-runtime-contract.sh
tests/test-bxapt-recovery.sh
```

`tools/build.sh` produces `build/bionicx-debug.apk` and the Android-compatible
glibc 2.41 overlay. It does not download Chrome or WPS.

## Seed and first install

```sh
tools/build-rootfs-seed.sh build/rootfs-seed-bundle
tools/install-apk.sh --serial "$SERIAL" build/bionicx-debug.apk
tools/install-profile.sh \
  --profile profiles/hello.json \
  --runtime-root build/rootfs-seed-bundle/rootfs \
  --serial "$SERIAL"
```

`tools/install-apk.sh` uses `adb install -r -t` on ordinary devices. On vivo
X300 / V2509A OriginOS it pushes the APK and taps the package-installer risk
page (`607,2289` then `607,2462` at 1216x2640) while `pm install -r -t` runs,
then stages `files/bin/bionicx-exec` so `bxapt normalize` does not wait for a
first Activity launch.

Pass the Debian tree (`.../rootfs`), not the seed bundle directory. The
installer rejects a nested bundle so `bxapt` can find `/usr/bin/apt-get`.

The seed id is `files/rootfs/.bionicx-rootfs-seed-id`. Rebuild the seed after
changing the Android glibc identity patches; the overlay libc at
`usr/lib/libc.so.6` must contain `BIONICX_VIRTUAL_ROOT`.

## Package the shared desktop

```sh
tools/bxapt --serial "$SERIAL" update
tools/bxapt --serial "$SERIAL" set packages/trixie-popular.txt
tools/bxapt --serial "$SERIAL" install cups-daemon cups-client
tools/bxapt --serial "$SERIAL" deb \
  google-chrome-stable_151.0.7922.108-1_arm64.deb \
  23f5d27be6ad6f5d69c1c11b602d4ed25a8499cfdfa11c3ca479ad0b58285499
tools/bxapt --serial "$SERIAL" deb \
  wps-office_11.1.0.11720_arm64.deb \
  172d8bcf3e3bc756994ae5adf66f63f22691e1ab0d18986c50bb6b6ab7f62948
tools/bxapt --serial "$SERIAL" deb \
  libwebp6_0.6.1-2.1+deb11u2_arm64.deb \
  edeb260e528fecae77457a63a468e55837a98079fdd7f1e20e9813c358f8c755
tools/bxapt --serial "$SERIAL" deb \
  libtiff5_4.2.0-1+deb11u5_arm64.deb \
  6896296ef6193ff77434c5d1d09dd9a333633f7a208ab1cc7de3b286d2d45824
tools/bxapt --serial "$SERIAL" dpkg --audit
```

Hashes live in `packages/external-arm64.tsv`. Proprietary `.deb` files are
never committed. After every successful transaction `dpkg --audit` must be
empty. Applications share `files/rootfs`; `files/apps/<id>` must not grow a
second `libc.so.6`.

Interrupted installs are finished with `tools/bxapt recover`.

## Desktop use

Install a profile without replacing the rootfs:

```sh
tools/install-profile.sh --profile profiles/xfce-session.json --serial "$SERIAL"
adb -s "$SERIAL" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity
```

`hostServices` may include `dbus`, `pulseaudio`, `cups`, and `vulkan`.
CUPS listens on the app-private socket exported as `CUPS_SERVER`. Create the
controlled destination once:

```sh
tools/bxapt --serial "$SERIAL" run lpadmin -p bionicx-test -E \
  -v file:///data/user/0/io.taowen.bx/files/run/cups/spool/test.out \
  -m raw
```

## Remaining Android-kernel limits

These are not hidden and will not be fixed by another profile:

- App seccomp rejects `set_robust_list` and `clone3`. Only the pinned Android
  glibc 2.41 overlay starts. Process-shared and PI robust mutexes stay
  unsupported.
- SysV message queues and some IPC remain blocked; BionicX supplies a
  file-backed semaphore namespace only.
- There is no mount namespace, systemd, system D-Bus, or desktop portal.
- MIT-SHM Attach, GetImage, CreatePixmap, and software PresentPixmap are
  implemented. Incomplete DRI3 stays hidden.
- The experimental APK uses targetSdk 28 so extracted app-data can be
  executed. A modern target SDK needs a different packaging design.
- 4 KiB page results do not imply 16 KiB compatibility.
- Chrome keeps `--no-sandbox` on purpose.
- Proprietary fonts are not bundled. Fontconfig aliases Microsoft family
  names to Liberation only after glyph coverage is verified.
