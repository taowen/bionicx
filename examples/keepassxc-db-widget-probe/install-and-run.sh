#!/usr/bin/env bash
# KeePassXC DatabaseWidget-shaped QStackedWidget/QSplitter tree, then the
# real keepassxc kdbx open path. App-only: do not replace the seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
bundle="$repo_dir/build/keepassxc-db-widget-probe-bundle"

mkdir -p "$bundle/app/bin"
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
            examples/keepassxc-db-widget-probe/keepassxc-db-widget-probe.cpp \
            -o build/keepassxc-db-widget-probe-bundle/app/bin/keepassxc-db-widget-probe \
            $(pkg-config --libs Qt5Widgets)
    '
chown "$(id -u):$(id -g)" \
    "$bundle/app/bin/keepassxc-db-widget-probe"
patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" \
    --set-rpath '$ORIGIN/../lib:/data/user/0/io.taowen.bx/files/rootfs/usr/lib/aarch64-linux-gnu' \
    "$bundle/app/bin/keepassxc-db-widget-probe"

"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/keepassxc-db-widget-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"

"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop "$package_id"
"${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
sleep 0.3
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity" >/dev/null

for _ in $(seq 1 80); do
    if "${adb[@]}" logcat -d -v brief | grep -Fq 'BXSUMMARY keepassxc-db-widget'; then
        break
    fi
    sleep 0.25
done
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|keepassxc-db-widget-probe exited')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY keepassxc-db-widget passed=4 failed=0" <<<"$result"
grep -Fq "keepassxc-db-widget-probe exited with 0" <<<"$result"

# Real KeePassXC: map first, then D-Bus open the fixture. Command-line
# --keyfile before bringToFront() is the NULL d_ptr 139 path.
"$repo_dir/examples/keepassxc-cli-probe/install-and-run.sh"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/keepassxc.json" \
    --serial "$serial"

screenshot="${BIONICX_SCREENSHOT:-$repo_dir/build/keepassxc-db-widget-device.png}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop "$package_id"
"${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
sleep 0.3
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity" >/dev/null

alive=0
for _ in $(seq 1 20); do
    if "${adb[@]}" shell "run-as $package_id sh -c 'pidof keepassxc'" \
            | grep -q '[0-9]'; then
        alive=1
        break
    fi
    sleep 0.5
done
# Wait for the deferred D-Bus open to switch to the unlocked view.
sleep 4
if ! "${adb[@]}" shell "run-as $package_id sh -c 'pidof keepassxc'" \
        | grep -q '[0-9]'; then
    alive=0
fi
"${adb[@]}" exec-out screencap -p > "$screenshot"
bytes="$(wc -c < "$screenshot" | tr -d ' ')"
echo "screenshot $screenshot bytes=$bytes alive=$alive"
"${adb[@]}" logcat -d -v brief | grep -E \
    'BionicX|keepassxc|fatal|exited|signal=' | tail -n 40
if [[ "$alive" -ne 1 ]]; then
    echo "keepassxc deferred open died" >&2
    exit 1
fi
if "${adb[@]}" logcat -d -v brief | grep -Fq 'keepassxc exited with 139'; then
    echo "keepassxc still SIGSEGV 139 after deferred open" >&2
    exit 1
fi
if [[ "$bytes" -lt 55000 ]]; then
    echo "keepassxc screenshot too small for unlocked database UI" >&2
    exit 1
fi
echo "BXTEST PASS keepassxc-deferred-open alive screenshot=$bytes"
echo "BXSUMMARY keepassxc-db-widget-gui passed=1 failed=0"
