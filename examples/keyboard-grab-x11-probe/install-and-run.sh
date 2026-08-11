#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_KEYBOARD_GRAB_BUNDLE:-$repo_dir/build/keyboard-grab-x11-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

"$repo_dir/examples/keyboard-grab-x11-probe/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh"
    --profile "$repo_dir/profiles/keyboard-grab-x11-probe.json"
    --app-root "$bundle_dir/app" --runtime-root "$bundle_dir/rootfs")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null

wait_log() {
    local pattern="$1"
    for _ in $(seq 1 100); do
        if "${adb[@]}" logcat -d -v brief | grep -Fq "$pattern"; then return 0; fi
        sleep 0.1
    done
    echo "timed out waiting for device log: $pattern" >&2
    return 1
}

wait_log "BXREADY keyboard-grab inject-a"
"${adb[@]}" shell input keyevent 29
wait_log "BXREADY keyboard-ungrab inject-b"
"${adb[@]}" shell input keyevent 30
wait_log "BXREADY keyboard-owner-events inject-c"
"${adb[@]}" shell input keyevent 31
wait_log "BXSUMMARY keyboard-grab-x11"
if [[ -n "${BIONICX_SCREENSHOT:-}" ]]; then
    "${adb[@]}" exec-out screencap -p > "$BIONICX_SCREENSHOT"
fi
wait_log "keyboard-grab-x11-probe exited with 0"
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(READY|TEST|SUMMARY|ERROR)|keyboard-grab-x11-probe exited with')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY keyboard-grab-x11 passed=7/7 xerrors=0" <<<"$result"
grep -Fq "keyboard-grab-x11-probe exited with 0" <<<"$result"
