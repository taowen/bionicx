#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_KEYBOARD_GRAB_BUNDLE:-$repo_dir/build/keyboard-grab-x11-probe-bundle}"

"$repo_dir/examples/keyboard-grab-x11-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/keyboard-grab-x11-probe.json" \
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

wait_log "BXREADY keyboard-grab inject-a"
adb -s "$serial" shell input keyevent 29
wait_log "BXREADY keyboard-ungrab inject-b"
adb -s "$serial" shell input keyevent 30
wait_log "BXREADY keyboard-owner-events inject-c"
adb -s "$serial" shell input keyevent 31
wait_log "BXREADY passive-key-grab inject-d"
adb -s "$serial" shell input keyevent 32
wait_log "BXREADY passive-key-release inject-e"
adb -s "$serial" shell input keyevent 33
wait_log "BXSUMMARY keyboard-grab-x11"
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BX(READY|TEST|SUMMARY|ERROR)')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY keyboard-grab-x11 passed=9/9 xerrors=0" <<<"$result"
echo "keyboard-grab X11 probe: PASS"
