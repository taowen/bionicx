#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_SESSION_X11_BUNDLE:-$repo_dir/build/session-x11-probe-bundle}"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

"$repo_dir/examples/session-x11-probe/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh"
    --profile "$repo_dir/profiles/session-x11-probe.json"
    --app-root "$bundle_dir/app" --runtime-root "$bundle_dir/rootfs")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity

deadline=$((SECONDS + 35))
while (( SECONDS < deadline )); do
    result="$("${adb[@]}" logcat -d -s BionicX)"
    grep -Fq 'BXSUMMARY session-x11 passed=3/3' <<<"$result" && break
    sleep 1
done
grep -F 'BXTEST PASS session-x11' <<<"$result"
grep -F 'bionicx-exec: primary ' <<<"$result"
grep -F 'bionicx-exec: session drained children=' <<<"$result"
grep -Fq 'BXSUMMARY session-x11 passed=3/3' <<<"$result"
