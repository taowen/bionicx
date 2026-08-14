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

run_profile vulkan-frames.json
wait_log "vulkan-frames first="
frames_shot="${BIONICX_FRAMES_SCREENSHOT:-}"
remove_frames=false
if [[ -z "$frames_shot" ]]; then
    frames_shot="$(mktemp --suffix=-bionicx-frames.png)"
    remove_frames=true
fi
trap '$remove_screenshot && rm -f "$screenshot"; $remove_frames && rm -f "$frames_shot"' EXIT
frames_ok=0
for _ in $(seq 1 80); do
    if "${adb[@]}" exec-out screencap -p > "$frames_shot" &&
            "$repo_dir/examples/vulkan-probe/assert-frames.py" "$frames_shot" \
                > "$repo_dir/build/vulkan-frames-result.log"; then
        frames_ok=1
        break
    fi
    if "${adb[@]}" logcat -d -v brief | grep -Fq "vulkan-frames exited with"; then
        break
    fi
    sleep 0.05
done
wait_log "BXTEST PASS vulkan-frames first="
cat "$repo_dir/build/vulkan-frames-result.log"
grep -Fq "BXTEST PASS host-vulkan-frames" \
    "$repo_dir/build/vulkan-frames-result.log"
wait_log "BXSUMMARY vulkan-frames passed=1 failed=0"
wait_log "vulkan-frames exited with 0"

run_profile vulkan-chrome-frames.json
wait_log "vulkan-chrome-frames extent="
chrome_shot="${BIONICX_CHROME_FRAMES_SCREENSHOT:-}"
remove_chrome=false
if [[ -z "$chrome_shot" ]]; then
    chrome_shot="$(mktemp --suffix=-bionicx-chrome-frames.png)"
    remove_chrome=true
fi
trap '$remove_screenshot && rm -f "$screenshot"; $remove_frames && rm -f "$frames_shot"; $remove_chrome && rm -f "$chrome_shot"' EXIT
chrome_ok=0
for _ in $(seq 1 80); do
    if "${adb[@]}" exec-out screencap -p > "$chrome_shot" &&
            "$repo_dir/examples/vulkan-probe/assert-frames.py" "$chrome_shot" \
                > "$repo_dir/build/vulkan-chrome-frames-result.log"; then
        chrome_ok=1
        break
    fi
    if "${adb[@]}" logcat -d -v brief | grep -Fq "vulkan-chrome-frames exited with"; then
        break
    fi
    sleep 0.05
done
wait_log "BXTEST PASS vulkan-chrome-frames extent="
cat "$repo_dir/build/vulkan-chrome-frames-result.log"
grep -Fq "BXTEST PASS host-vulkan-frames" \
    "$repo_dir/build/vulkan-chrome-frames-result.log"
wait_log "BXSUMMARY vulkan-chrome-frames passed=1 failed=0"
wait_log "vulkan-chrome-frames exited with 0"

run_profile vulkan-lifetime.json
wait_log "BXTEST PASS vulkan-lifetime-deferred-fence"
wait_log "BXSUMMARY vulkan-lifetime passed=2 failed=0"
wait_log "vulkan-lifetime exited with 0"

result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|vulkan-(wsi|present|frames|chrome-frames|lifetime) exited|enabled Vulkan')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY vulkan-lifetime passed=1 failed=0" <<<"$result"
echo "BXSUMMARY vulkan-probes wsi=5 present=3 frames=1 chrome-frames=1 lifetime=2 compositor=3"
