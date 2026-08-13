#!/usr/bin/env bash
# LibreOffice Writer document load / --convert-to. App-only: do not replace
# the shared seed.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
package_id=io.taowen.bx
root="/data/user/0/$package_id/files/rootfs"
bundle="$repo_dir/build/soffice-doc-probe-bundle"

mkdir -p "$bundle/app/bin" "$bundle/app/fixtures"
python3 "$repo_dir/examples/productivity-apps/build-odt-fixture.py" \
    "$bundle/app/fixtures/bionicx-writer.odt"
builder="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --network host --userns=keep-id \
    --volume "$repo_dir:/work:Z" --workdir /work \
    "$builder" aarch64-linux-gnu-gcc -O2 -Wall -Wextra -Werror \
        examples/soffice-doc-probe/soffice-doc-probe.c \
        -o build/soffice-doc-probe-bundle/app/bin/soffice-doc-probe
patchelf --set-interpreter "$root/usr/lib/ld-linux-aarch64.so.1" \
    --set-rpath "$root/usr/lib:$root/usr/lib/aarch64-linux-gnu" \
    "$bundle/app/bin/soffice-doc-probe"

"$repo_dir/tools/install-profile.sh" \
    --profile "$repo_dir/profiles/soffice-doc-probe.json" \
    --app-root "$bundle/app" \
    --serial "$serial"

"${adb[@]}" logcat -c
"${adb[@]}" shell am force-stop "$package_id"
"${adb[@]}" shell "run-as $package_id sh -c 'kill -9 \$(pidof bionicx-exec) 2>/dev/null; true'"
sleep 0.3
"${adb[@]}" shell am start -W \
    -n "$package_id/com.winlator.BionicXActivity" >/dev/null

for _ in $(seq 1 120); do
    if "${adb[@]}" logcat -d -v brief | grep -Fq 'BXSUMMARY soffice-doc'; then
        break
    fi
    sleep 0.5
done
result="$("${adb[@]}" logcat -d -v brief \
    | grep -E 'BX(TEST|SUMMARY)|soffice-doc-probe exited|WrappedTarget|Unspecified')"
printf '%s\n' "$result"
grep -Fq "BXSUMMARY soffice-doc passed=8 failed=0" <<<"$result"
grep -Fq "soffice-doc-probe exited with 0" <<<"$result"
