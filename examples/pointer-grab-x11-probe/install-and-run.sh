#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_POINTER_GRAB_BUNDLE:-$repo_dir/build/pointer-grab-x11-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

"$repo_dir/examples/pointer-grab-x11-probe/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh"
    --profile "$repo_dir/profiles/pointer-grab-x11-probe.json"
    --app-root "$bundle_dir/app" --runtime-root "$bundle_dir/rootfs")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null

wait_log() {
    local pattern="$1"
    for _ in $(seq 1 100); do
        if "${adb[@]}" logcat -d -v brief | grep -Fq "$pattern"; then return 0; fi
        sleep 0.1
    done
    echo "timed out waiting for device log: $pattern" >&2
    return 1
}

wait_log "BXREADY passive-button-grab tap-peer"
# The X client can become ready while Android's activity-open transition is
# still consuming touches. Keep that UI transition out of the protocol test.
sleep 0.3
"${adb[@]}" shell input tap 1050 260
wait_log "BXREADY passive-button-owner-events tap-grabber"
"${adb[@]}" shell input tap 300 260
wait_log "BXREADY passive-button-ungrab tap-peer"
"${adb[@]}" shell input tap 1050 260
wait_log "BXSUMMARY pointer-grab-x11"
wait_log "pointer-grab-x11-probe exited with 0"
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(READY|EVENT|TEST|SUMMARY|ERROR)|pointer-grab-x11-probe exited with')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY pointer-grab-x11 passed=5/5 xerrors=0" <<<"$result"
grep -Fq "pointer-grab-x11-probe exited with 0" <<<"$result"
