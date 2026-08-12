#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_VULKAN_BUNDLE:-$repo_dir/build/vulkan-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

"$repo_dir/examples/vulkan-probe/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh"
    --profile "$repo_dir/profiles/vulkan-probe.json"
    --app-root "$bundle_dir/app" --runtime-root "$bundle_dir/rootfs")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
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

wait_log "BXTEST PASS host-vulkan-present status="
screenshot="${BIONICX_SCREENSHOT:-}"
remove_screenshot=false
if [[ -z "$screenshot" ]]; then
    screenshot="$(mktemp --suffix=-bionicx-vulkan.png)"
    remove_screenshot=true
fi
trap '$remove_screenshot && rm -f "$screenshot"' EXIT
for _ in $(seq 1 30); do
    "${adb[@]}" exec-out screencap -p > "$screenshot"
    if "$repo_dir/examples/vulkan-probe/assert-screenshot.py" "$screenshot" \
            > "$repo_dir/build/vulkan-compositor-result.log"; then
        break
    fi
    sleep 0.1
done
cat "$repo_dir/build/vulkan-compositor-result.log"
grep -Fq "BXTEST PASS host-vulkan-compositor" \
    "$repo_dir/build/vulkan-compositor-result.log"

wait_log "BXSUMMARY host-vulkan"
wait_log "vulkan-probe exited with 0"

result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|vulkan-probe exited with|enabled Vulkan')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY host-vulkan passed=23 failed=0" <<<"$result"
grep -Fq "vulkan-probe exited with 0" <<<"$result"
