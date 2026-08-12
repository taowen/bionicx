#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_GLX_BUNDLE:-$repo_dir/build/glx-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

"$repo_dir/examples/glx-probe/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh"
    --profile "$repo_dir/profiles/glx-probe.json"
    --app-root "$bundle_dir/app" --runtime-root "$bundle_dir/rootfs")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null

wait_log() {
    local pattern="$1"
    for _ in $(seq 1 300); do
        if "${adb[@]}" logcat -d -v brief | grep -Fq "$pattern"; then return 0; fi
        sleep 0.1
    done
    echo "timed out waiting for device log: $pattern" >&2
    return 1
}

wait_log "BXTEST PASS glx-present"
screenshot="${BIONICX_SCREENSHOT:-}"
remove_screenshot=false
if [[ -z "$screenshot" ]]; then
    screenshot="$(mktemp --suffix=-bionicx-glx.png)"
    remove_screenshot=true
fi
trap '$remove_screenshot && rm -f "$screenshot"' EXIT
for _ in $(seq 1 30); do
    "${adb[@]}" exec-out screencap -p > "$screenshot"
    if "$repo_dir/examples/glx-probe/assert-screenshot.py" "$screenshot" \
            > "$repo_dir/build/glx-compositor-result.log"; then
        break
    fi
    sleep 0.1
done
cat "$repo_dir/build/glx-compositor-result.log"
grep -Fq "BXTEST PASS host-gl-compositor" \
    "$repo_dir/build/glx-compositor-result.log"

wait_log "BXSUMMARY host-glx"
wait_log "glx-probe exited with 0"
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|glx-probe exited with')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY host-glx passed=25 failed=0" <<<"$result"
grep -Fq "glx-probe exited with 0" <<<"$result"
