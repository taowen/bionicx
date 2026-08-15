#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_X11_DESKTOP_BUNDLE:-$repo_dir/build/x11-desktop-probe-bundle}"

"$repo_dir/examples/x11-desktop-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/x11-desktop-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
sleep 1
adb -s "$serial" shell input tap 650 200
sleep 1
adb -s "$serial" shell input tap 180 200
sleep 1
adb -s "$serial" shell input text abc_A
for i in $(seq 1 15); do
    if adb -s "$serial" logcat -d -v brief \
            | grep -Fq 'BXSUMMARY desktop-x11'; then
        echo "x11-desktop probe finished at ${i}s"
        break
    fi
    sleep 1
done
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BXTEST|BXSUMMARY|BXERROR|BXCAP')"
printf '%s\n' "$result"
grep -Eq 'BXSUMMARY desktop-x11 passed=[0-9]+ failed=0' <<<"$result"
echo "desktop X11 probe: PASS"
