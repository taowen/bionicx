#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_RESIZE_SCREEN_BUNDLE:-$repo_dir/build/resize-screen-x11-probe-bundle}"
shot="${BIONICX_RESIZE_SCREEN_SHOT:-$repo_dir/build/resize-screen-x11.png}"

"$repo_dir/examples/resize-screen-x11-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/resize-screen-x11-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
for i in $(seq 1 30); do
    if adb -s "$serial" logcat -d -v brief \
            | grep -Fq 'BXSUMMARY resize-screen-x11'; then
        echo "resize-screen-x11 probe finished at ${i}s"
        break
    fi
    sleep 1
done
adb -s "$serial" exec-out screencap -p > "$shot"
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BXTEST|BXSUMMARY|BXERROR|BXGEOM')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY resize-screen-x11 passed=5 failed=0" <<<"$result"
python3 "$repo_dir/examples/resize-screen-x11-probe/assert-screenshot.py" \
    "$shot" "34,102,204"
echo "resize-screen X11 probe: PASS"
