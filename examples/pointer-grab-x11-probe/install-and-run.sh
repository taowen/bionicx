#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_POINTER_GRAB_BUNDLE:-$repo_dir/build/pointer-grab-x11-probe-bundle}"

"$repo_dir/examples/pointer-grab-x11-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/pointer-grab-x11-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null

wait_log() {
    local pattern="$1"
    for _ in $(seq 1 100); do
        if adb -s "$serial" logcat -d -s BionicX -t 80 \
                | grep -Fq "$pattern"; then
            return 0
        fi
        sleep 0.1
    done
    echo "timed out waiting for device log: $pattern" >&2
    return 1
}

wait_log "BXREADY passive-button-grab tap-peer"
sleep 0.4
adb -s "$serial" shell input tap 1050 260
wait_log "BXREADY passive-button-replay tap-peer"
adb -s "$serial" shell input tap 1050 260
wait_log "BXREADY passive-button-owner-events tap-grabber"
adb -s "$serial" shell input tap 300 260
wait_log "BXREADY passive-button-ungrab tap-peer"
adb -s "$serial" shell input tap 1050 260
wait_log "BXSUMMARY pointer-grab-x11"
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BX(READY|EVENT|TEST|SUMMARY|ERROR)')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY pointer-grab-x11 passed=6/6 xerrors=0" <<<"$result"
echo "pointer-grab X11 probe: PASS"
