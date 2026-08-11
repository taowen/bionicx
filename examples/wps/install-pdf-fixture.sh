#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )
package="io.taowen.bx"
document="BionicX-PDF-Integration.pdf"
device_path="files/homes/wps-office/Documents/$document"
work_dir="$(mktemp -d)"
fixture="$work_dir/$document"
temporary="/data/local/tmp/bionicx-pdf-fixture-$$.pdf"
trap 'rm -rf "$work_dir"; "${adb[@]}" shell rm -f "$temporary" >/dev/null 2>&1 || true' EXIT

python3 "$repo_dir/examples/wps/build-pdf-fixture.py" "$fixture"
pages="$(pdfinfo "$fixture" | awk '/^Pages:/ {print $2}')"
[[ "$pages" == "2" ]] || {
    echo "wrong generated PDF page count: $pages" >&2
    exit 1
}
text="$(pdftotext "$fixture" - | tr '\f' '\n')"
for expected in \
        'BionicX PDF Page 1' 'glibc + X11 on Android' \
        'BionicX PDF Page 2' 'Navigation verified'; do
    grep -Fqx "$expected" <<< "$text" || {
        echo "missing generated PDF text: $expected" >&2
        exit 1
    }
done

"${adb[@]}" push "$fixture" "$temporary" >/dev/null
"${adb[@]}" shell run-as "$package" mkdir -p files/homes/wps-office/Documents
"${adb[@]}" shell run-as "$package" cp "$temporary" "$device_path"
"${adb[@]}" shell run-as "$package" chmod 600 "$device_path"
host_hash="$(sha256sum "$fixture" | cut -d' ' -f1)"
device_hash="$("${adb[@]}" exec-out run-as "$package" \
    sha256sum "$device_path" | cut -d' ' -f1)"
[[ "$device_hash" == "$host_hash" ]] || {
    echo "device PDF fixture hash mismatch" >&2
    exit 1
}
printf 'BXFIXTURE pdf=%s pages=%s bytes=%s sha256=%s\n' \
    "$document" "$pages" "$(stat -c %s "$fixture")" "$host_hash"
