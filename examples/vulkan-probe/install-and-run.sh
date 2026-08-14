#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_VULKAN_BUNDLE:-$repo_dir/build/vulkan-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )
package_id=io.taowen.bx

"$repo_dir/examples/vulkan-probe/build-bundle.sh" "$bundle_dir"

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

run_profile() {
    local profile="$1"
    local install=("$repo_dir/tools/install-profile.sh"
        --profile "$repo_dir/profiles/$profile" --app-root "$bundle_dir/app")
    [[ -z "$serial" ]] || install+=(--serial "$serial")
    "${install[@]}"
    "${adb[@]}" logcat -c
    "${adb[@]}" shell am force-stop "$package_id"
    "${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
    sleep 0.3
    "${adb[@]}" shell am start -W \
        -n "$package_id/com.winlator.BionicXActivity" >/dev/null
    "${adb[@]}" shell cmd statusbar collapse >/dev/null 2>&1 || true
}

# App-only install. The compact hello rootfs in the bundle must not replace
# the device seed; libc/loader come from the shared rootfs after normalize.
run_profile vulkan-wsi.json
wait_log "BXSUMMARY vulkan-wsi passed=5 failed=0"
wait_log "vulkan-wsi exited with 0"

run_profile vulkan-present.json
wait_log "vulkan-present status=0"

screenshot="${BIONICX_SCREENSHOT:-}"
remove_screenshot=false
if [[ -z "$screenshot" ]]; then
    screenshot="$(mktemp --suffix=-bionicx-vulkan.png)"
    remove_screenshot=true
fi
trap '$remove_screenshot && rm -f "$screenshot"' EXIT
compositor_ok=0
for _ in $(seq 1 80); do
    if "${adb[@]}" exec-out screencap -p > "$screenshot" &&
            "$repo_dir/examples/vulkan-probe/assert-screenshot.py" "$screenshot" \
                > "$repo_dir/build/vulkan-compositor-result.log"; then
        compositor_ok=1
        break
    fi
    if "${adb[@]}" logcat -d -v brief | grep -Fq "vulkan-present exited with"; then
        break
    fi
    sleep 0.05
done
wait_log "BXTEST PASS vulkan-present status="
cat "$repo_dir/build/vulkan-compositor-result.log"
grep -Fq "BXTEST PASS host-vulkan-compositor" \
    "$repo_dir/build/vulkan-compositor-result.log"
wait_log "BXSUMMARY vulkan-present passed=3 failed=0"
wait_log "vulkan-present exited with 0"

run_profile vulkan-lifetime.json
wait_log "BXSUMMARY vulkan-lifetime passed=1 failed=0"
wait_log "vulkan-lifetime exited with 0"

result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|vulkan-(wsi|present|lifetime) exited|enabled Vulkan')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY vulkan-lifetime passed=1 failed=0" <<<"$result"
echo "BXSUMMARY vulkan-probes wsi=5 present=3 lifetime=1 compositor=1"
