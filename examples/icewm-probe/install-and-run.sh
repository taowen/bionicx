#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_ICEWM_BUNDLE:-$repo_dir/build/icewm-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb=("${ADB:-$HOME/Android/Sdk/platform-tools/adb}")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

"$repo_dir/examples/icewm-probe/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh" --profile \
    "$repo_dir/profiles/icewm-probe.json" --app-root "$bundle_dir/app" \
    --runtime-root "$bundle_dir/rootfs")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null

for _ in $(seq 1 100); do
    "${adb[@]}" logcat -d -v brief | grep -Fq \
        'BXTEST PASS icewm-manager-start' && break
    sleep 0.1
done

for _ in $(seq 1 250); do
    "${adb[@]}" logcat -d -v brief | grep -Fq \
        'BXSUMMARY icewm passed=4 failed=0' && break
    sleep 0.1
done
result="$("${adb[@]}" logcat -d -v brief | grep -E \
    'BXICEWM|BXTEST|BXSUMMARY|icewm-probe exited with')"
printf '%s\n' "$result"
grep -Fq 'BXSUMMARY icewm passed=4 failed=0' <<<"$result"
grep -Fq 'icewm-probe exited with 0' <<<"$result"
