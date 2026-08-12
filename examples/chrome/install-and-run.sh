#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bundle_dir="${BIONICX_CHROME_BUNDLE:-$repo_dir/build/chrome-bundle}"
profile="${BIONICX_CHROME_PROFILE:-$repo_dir/profiles/chrome-smoke.json}"
serial="${ANDROID_SERIAL:-}"

TMPDIR="$repo_dir/build/tmp" \
    "$repo_dir/examples/chrome/build-bundle.sh" "$bundle_dir"
install=("$repo_dir/tools/install-profile.sh"
    --profile "$profile"
    --app-root "$bundle_dir/app" --runtime-root "$bundle_dir/rootfs")
[[ -z "$serial" ]] || install+=(--serial "$serial")
"${install[@]}"
ANDROID_SERIAL="$serial" "$repo_dir/examples/chrome/install-open-fonts.sh"

adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )
"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop io.taowen.bx
"${adb[@]}" shell am start -W -n io.taowen.bx/com.winlator.BionicXActivity
