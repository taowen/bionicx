#!/usr/bin/env bash
# Minimal Qt5 QWidget::show() client. Reproduces QWidgetPrivate::showChildren
# NULL d_ptr SIGSEGV. App-only: do not replace the seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
bundle="$repo_dir/build/qt-widgets-show-probe-bundle"

mkdir -p "$bundle/app/bin"
# Widgets show() must not pull Gladio: that libGL is missing XQueryExtension
# unless the main executable also links -lX11. KeePassXC dies in Qt Widgets.

builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" sh -eu -c '
        apt-get update >/dev/null
        DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            g++-aarch64-linux-gnu pkg-config qtbase5-dev:arm64 >/dev/null
        export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig
        aarch64-linux-gnu-g++ -O2 -Wall -Wextra -Werror -fPIC \
            $(pkg-config --cflags Qt5Widgets) \
            examples/qt-widgets-show-probe/qt-widgets-show-probe.cpp \
            -o build/qt-widgets-show-probe-bundle/app/bin/qt-widgets-show-probe \
            $(pkg-config --libs Qt5Widgets)
    '
chown "$(id -u):$(id -g)" \
    "$bundle/app/bin/qt-widgets-show-probe"
patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" \
    --set-rpath '$ORIGIN/../lib:/data/user/0/io.taowen.bx/files/rootfs/usr/lib/aarch64-linux-gnu' \
    "$bundle/app/bin/qt-widgets-show-probe"

"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/qt-widgets-show-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"

"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop "$package_id"
"${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
sleep 0.3
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity" >/dev/null

for _ in $(seq 1 80); do
    if "${adb[@]}" logcat -d -v brief | grep -Fq 'BXSUMMARY qt-widgets-show'; then
        break
    fi
    sleep 0.25
done
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|qt-widgets-show-probe exited')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY qt-widgets-show passed=3 failed=0" <<<"$result"
grep -Fq "qt-widgets-show-probe exited with 0" <<<"$result"
