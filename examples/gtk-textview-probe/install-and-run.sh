#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_GTK_TEXTVIEW_BUNDLE:-$repo_dir/build/gtk-textview-probe-bundle}"

"$repo_dir/examples/gtk-textview-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/gtk-textview-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
bionicx_log="${BIONICX_GTK_TEXTVIEW_LOG:-$repo_dir/build/gtk-textview-bionicx.log}"
: > "$bionicx_log"
adb -s "$serial" logcat -v brief > "$bionicx_log" &
logcat_pid=$!
trap 'kill "$logcat_pid" 2>/dev/null || true' EXIT
sleep 2
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
for i in $(seq 1 30); do
    if grep -Fq 'BXSUMMARY gtk-textview' "$bionicx_log" \
            || adb -s "$serial" logcat -d -v brief \
                | grep -Fq 'BXSUMMARY gtk-textview'; then
        echo "gtk-textview probe finished at ${i}s"
        break
    fi
    sleep 1
done
sleep 1
kill "$logcat_pid" 2>/dev/null || true
wait "$logcat_pid" 2>/dev/null || true
trap - EXIT
log="$(cat "$bionicx_log"; adb -s "$serial" logcat -d -v brief)"
result="$(grep -E 'BXTEST|BXSUMMARY|BXINFO tv-' <<<"$log" | awk '!seen[$0]++')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY gtk-textview passed=6 failed=0" <<<"$result"
echo "gtk-textview probe: PASS"
