#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_SESSION_X11_BUNDLE:-$repo_dir/build/session-x11-probe-bundle}"

"$repo_dir/examples/session-x11-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/session-x11-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
for i in $(seq 1 35); do
    if adb -s "$serial" logcat -d -v brief \
            | grep -Fq 'BXSUMMARY session-x11 passed=3/3'; then
        echo "session-x11 probe finished at ${i}s"
        break
    fi
    sleep 1
done
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BXTEST|BXSUMMARY|BXERROR|bionicx-exec:')"
printf '%s\n' "$result"
grep -Fq 'BXSUMMARY session-x11 passed=3/3' <<<"$result"
echo "session X11 probe: PASS"
