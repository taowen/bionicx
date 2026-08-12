#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_SAVE_SET_BUNDLE:-$repo_dir/build/save-set-x11-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )
"$repo_dir/examples/save-set-x11-probe/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh"
    --profile "$repo_dir/profiles/save-set-x11-probe.json"
    --app-root "$bundle_dir/app" --runtime-root "$bundle_dir/rootfs")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
for _ in $(seq 1 150); do
    if "${adb[@]}" logcat -d -v brief \
            | grep -Fq "BXSUMMARY save-set-x11"; then break; fi
    sleep 0.1
done
for _ in $(seq 1 100); do
    if "${adb[@]}" logcat -d -v brief \
            | grep -Fq "save-set-x11-probe exited with 0"; then break; fi
    sleep 0.1
done
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY|ERROR)|save-set-x11-probe exited with')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY save-set-x11 passed=2/2 xerrors=0" <<<"$result"
grep -Fq "save-set-x11-probe exited with 0" <<<"$result"
