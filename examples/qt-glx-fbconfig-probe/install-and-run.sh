#!/usr/bin/env bash
# Mesa GLX client, same path as Krita. App-only: do not replace the seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
bundle="$repo_dir/build/qt-glx-fbconfig-probe-bundle"

builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
mkdir -p "$bundle/app/bin" "$bundle/app/lib"
"$repo_dir/tools/build-gladio.sh" "$bundle/app/lib"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
    -Ithird_party/gladio/include \
    examples/qt-glx-fbconfig-probe/qt-glx-fbconfig-probe.c \
    -Lbuild/qt-glx-fbconfig-probe-bundle/app/lib \
    -o build/qt-glx-fbconfig-probe-bundle/app/bin/qt-glx-fbconfig-probe \
    -lX11 -lGL
patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" \
    --set-rpath '$ORIGIN/../lib' \
    "$bundle/app/bin/qt-glx-fbconfig-probe"

"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/qt-glx-fbconfig-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"

"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop "$package_id"
"${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
sleep 0.3
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity" >/dev/null

for _ in $(seq 1 80); do
    if "${adb[@]}" logcat -d -v brief | grep -Fq 'BXSUMMARY qt-glx-fbconfig'; then
        break
    fi
    sleep 0.25
done
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|qt-glx-fbconfig-probe exited')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY qt-glx-fbconfig passed=6 failed=0" <<<"$result"
grep -Fq "qt-glx-fbconfig-probe exited with 0" <<<"$result"
