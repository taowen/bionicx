#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 DOCUMENT.pdf PAGES EXPECTED_TEXT ..." >&2
}

[[ $# -ge 3 ]] || { usage; exit 2; }
document="$1"
expected_pages="$2"
shift 2
[[ "$document" =~ ^[A-Za-z0-9._-]+\.pdf$ ]] || {
    echo "document must be a safe .pdf basename: $document" >&2
    exit 2
}
[[ "$expected_pages" =~ ^[1-9][0-9]*$ ]] || {
    echo "pages must be a positive integer" >&2
    exit 2
}

serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )
package="io.taowen.bx"
device_path="files/homes/wps-office/Documents/$document"
work_dir="$(mktemp -d)"
archive="$work_dir/$document"
trap 'rm -rf "$work_dir"' EXIT

"${adb[@]}" exec-out run-as "$package" cat "$device_path" > "$archive"
[[ -s "$archive" ]] || {
    echo "empty or missing device PDF: $device_path" >&2
    exit 1
}
actual_pages="$(pdfinfo "$archive" | awk '/^Pages:/ {print $2}')"
[[ "$actual_pages" == "$expected_pages" ]] || {
    echo "wrong PDF page count: expected $expected_pages, got $actual_pages" >&2
    exit 1
}
text="$(pdftotext "$archive" - | tr '\f' '\n')"
for expected in "$@"; do
    grep -Fqx "$expected" <<< "$text" || {
        echo "missing exact PDF text: $expected" >&2
        exit 1
    }
done
printf 'BXTEST PASS wps-pdf archive=%s pages=%s bytes=%s sha256=%s\n' \
    "$document" "$actual_pages" "$(stat -c %s "$archive")" \
    "$(sha256sum "$archive" | cut -d' ' -f1)"
