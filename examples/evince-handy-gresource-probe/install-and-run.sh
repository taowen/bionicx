#!/usr/bin/env bash
# libhandy theme GResource after ELF fixup. App-only: do not replace the seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
bundle="$repo_dir/build/evince-handy-gresource-probe-bundle"

mkdir -p "$bundle/app/bin"
builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/evince-handy-gresource-probe/evince-handy-gresource-probe.c \
        -o build/evince-handy-gresource-probe-bundle/app/bin/evince-handy-gresource-probe \
        -ldl
patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" \
    --set-rpath "$root/usr/lib:$root/usr/lib/aarch64-linux-gnu" \
    "$bundle/app/bin/evince-handy-gresource-probe"

"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/evince-handy-gresource-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"

"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop "$package_id"
"${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
sleep 0.3
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity" >/dev/null

for _ in $(seq 1 80); do
    if "${adb[@]}" logcat -d -v brief | grep -Fq 'BXSUMMARY evince-handy-gresource'; then
        break
    fi
    sleep 0.25
done
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|evince-handy-gresource-probe exited')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY evince-handy-gresource passed=6 failed=0" <<<"$result"
grep -Fq "evince-handy-gresource-probe exited with 0" <<<"$result"
