#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_X11_PROBE_BUNDLE:-$repo_dir/build/x11-probe-bundle}"

"$repo_dir/examples/x11-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/x11-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
sleep 1
adb -s "$serial" shell input text abc_A
adb -s "$serial" shell input tap 480 360
adb -s "$serial" shell input swipe 520 380 760 520 350
for i in $(seq 1 20); do
    if adb -s "$serial" logcat -d -v brief | grep -Fq 'BXSUMMARY passed='; then
        echo "x11-probe finished at ${i}s"
        break
    fi
    sleep 1
done
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BXTEST|BXSUMMARY|BXERROR|BXOBS|BXINFO unimplemented')"
printf '%s\n' "$result"
grep -Eq 'BXSUMMARY passed=[0-9]+ failed=0' <<<"$result"
echo "core X11 probe: PASS"
