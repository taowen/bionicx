#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_SEND_EVENT_BUNDLE:-$repo_dir/build/send-event-x11-probe-bundle}"

"$repo_dir/examples/send-event-x11-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/send-event-x11-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
for i in $(seq 1 30); do
    if adb -s "$serial" logcat -d -v brief \
            | grep -Fq 'BXSUMMARY send-event-x11'; then
        echo "send-event-x11 probe finished at ${i}s"
        break
    fi
    sleep 1
done
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BXTEST|BXSUMMARY|BXERROR')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY send-event-x11 passed=8 failed=0" <<<"$result"
echo "SendEvent X11 probe: PASS"
