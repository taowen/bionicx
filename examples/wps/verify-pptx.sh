#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 PRESENTATION.pptx EXPECTED_SLIDE_TEXT ..." >&2
}

[[ $# -ge 2 ]] || { usage; exit 2; }
document="$1"
shift
[[ "$document" =~ ^[A-Za-z0-9._-]+\.pptx$ ]] || {
    echo "document must be a safe .pptx basename: $document" >&2
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
    echo "empty or missing device presentation: $device_path" >&2
    exit 1
}

python3 - "$archive" "$@" <<'PY'
import sys
import xml.etree.ElementTree as ET
import zipfile

archive, *expected = sys.argv[1:]
with zipfile.ZipFile(archive) as pptx:
    bad = pptx.testzip()
    if bad is not None:
        raise SystemExit(f"corrupt PPTX member: {bad}")
    names = set(pptx.namelist())
    required = {
        "[Content_Types].xml",
        "_rels/.rels",
        "ppt/presentation.xml",
        "ppt/slides/slide1.xml",
    }
    missing = sorted(required - names)
    if missing:
        raise SystemExit(f"missing PPTX members: {', '.join(missing)}")
    slide = ET.fromstring(pptx.read("ppt/slides/slide1.xml"))

drawing_namespace = "http://schemas.openxmlformats.org/drawingml/2006/main"
texts = [
    node.text or ""
    for node in slide.iter(f"{{{drawing_namespace}}}t")
]
for value in expected:
    if value not in texts:
        raise SystemExit(
            f"missing exact slide text {value!r}; actual text runs: {texts!r}"
        )

print(
    f"BXTEST PASS wps-pptx archive={archive.rsplit('/', 1)[-1]} "
    f"members={len(names)} slide1_text_runs={len(texts)}"
)
for index, value in enumerate(texts, 1):
    print(f"BXSLIDE run={index} text={value}")
PY
