#!/usr/bin/env bash
# Installs only the probe payload. Does not replace the shared seed rootfs.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
bundle="${BIONICX_GTK_MENU_BUNDLE:-$repo_dir/build/gtk-menu-probe-bundle}"

"$repo_dir/examples/gtk-menu-probe/build-bundle.sh" "$bundle"
"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/gtk-menu-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"
adb -s "$serial" logcat -c
adb -s "$serial" shell am force-stop io.taowen.bx
adb -s "$serial" shell am start -W \
    -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
for i in $(seq 1 30); do
    if adb -s "$serial" logcat -d -v brief \
            | grep -Fq 'BXSUMMARY gtk-menu'; then
        echo "gtk-menu probe finished at ${i}s"
        break
    fi
    sleep 1
done
result="$(adb -s "$serial" logcat -d -v brief \
    | grep -E 'BXTEST|BXSUMMARY')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY gtk-menu passed=9 failed=0" <<<"$result"
echo "gtk-menu probe: PASS"
