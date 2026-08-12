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

for _ in $(seq 1 300); do
    if "${adb[@]}" logcat -d -v brief \
            | grep -Fq "BXSUMMARY host-vulkan"; then break; fi
    sleep 0.1
done

result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|vulkan-probe exited with|enabled Vulkan')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY host-vulkan passed=7 failed=0" <<<"$result"
grep -Fq "vulkan-probe exited with 0" <<<"$result"
