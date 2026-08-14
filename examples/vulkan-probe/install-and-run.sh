#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_VULKAN_BUNDLE:-$repo_dir/build/vulkan-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

"$repo_dir/examples/vulkan-probe/build-bundle.sh" "$bundle_dir"
# App-only install. The compact hello rootfs in the bundle must not replace
# the device seed; libc/loader come from the shared rootfs after normalize.
install=("$repo_dir/tools/install-profile.sh"
    --profile "$repo_dir/profiles/vulkan-probe.json"
    --app-root "$bundle_dir/app")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
# Detached run-as helpers (cupsd leftover) are not in ActivityManager.
"${adb[@]}" shell "run-as io.taowen.bx sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
sleep 0.3
"${adb[@]}" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
"${adb[@]}" shell cmd statusbar collapse >/dev/null 2>&1 || true

wait_log() {
    local pattern="$1"
    for _ in $(seq 1 300); do
        if "${adb[@]}" logcat -d -v brief | grep -Fq "$pattern"; then
            return 0
        fi
        sleep 0.1
    done
    echo "timed out waiting for device log: $pattern" >&2
    return 1
}

wait_log "host-vulkan-present status=0"

screenshot="${BIONICX_SCREENSHOT:-}"
remove_screenshot=false
if [[ -z "$screenshot" ]]; then
    screenshot="$(mktemp --suffix=-bionicx-vulkan.png)"
    remove_screenshot=true
fi
trap '$remove_screenshot && rm -f "$screenshot"' EXIT
# Screencap is slow; start grabbing as soon as the activity is up so the
# present-hold window is not missed.
compositor_ok=0
for _ in $(seq 1 80); do
    if "${adb[@]}" exec-out screencap -p > "$screenshot" &&
            "$repo_dir/examples/vulkan-probe/assert-screenshot.py" "$screenshot" \
                > "$repo_dir/build/vulkan-compositor-result.log"; then
        compositor_ok=1
        break
    fi
    if "${adb[@]}" logcat -d -v brief | grep -Fq "vulkan-probe exited with"; then
        break
    fi
    sleep 0.05
done
wait_log "BXTEST PASS host-vulkan-present status="
cat "$repo_dir/build/vulkan-compositor-result.log"
grep -Fq "BXTEST PASS host-vulkan-compositor" \
    "$repo_dir/build/vulkan-compositor-result.log"

wait_log "BXSUMMARY host-vulkan"
wait_log "vulkan-probe exited with 0"

result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|vulkan-probe exited with|enabled Vulkan')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY host-vulkan passed=43 failed=0" <<<"$result"
grep -Fq "vulkan-probe exited with 0" <<<"$result"
