#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 DOCUMENT.docx EXPECTED_TEXT ... [--bold TEXT ...]" >&2
}

[[ $# -ge 2 ]] || { usage; exit 2; }
document="$1"
shift
[[ "$document" =~ ^[A-Za-z0-9._-]+\.docx$ ]] || {
    echo "document must be a safe .docx basename: $document" >&2
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
    echo "empty or missing device document: $device_path" >&2
    exit 1
}

checks=()
while [[ $# -gt 0 ]]; do
    if [[ "$1" == "--bold" ]]; then
        [[ $# -ge 2 ]] || { usage; exit 2; }
        checks+=("bold=$2")
        shift 2
    else
        checks+=("text=$1")
        shift
    fi
done

python3 - "$archive" "${checks[@]}" <<'PY'
import sys
import xml.etree.ElementTree as ET
import zipfile

archive, *checks = sys.argv[1:]
with zipfile.ZipFile(archive) as docx:
    bad = docx.testzip()
    if bad is not None:
        raise SystemExit(f"corrupt DOCX member: {bad}")
    names = set(docx.namelist())
    required = {"[Content_Types].xml", "_rels/.rels", "word/document.xml"}
    missing = sorted(required - names)
    if missing:
        raise SystemExit(f"missing DOCX members: {', '.join(missing)}")
    root = ET.fromstring(docx.read("word/document.xml"))

namespace = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
paragraphs = []
bold_runs = []
for paragraph in root.findall(".//w:p", namespace):
    text = "".join(node.text or "" for node in paragraph.findall(".//w:t", namespace))
    if text:
        paragraphs.append(text)
    for run in paragraph.findall("w:r", namespace):
        run_text = "".join(node.text or "" for node in run.findall(".//w:t", namespace))
        bold = run.find("w:rPr/w:b", namespace)
        value = bold.get(f"{{{namespace['w']}}}val") if bold is not None else None
        if run_text and bold is not None and value not in {"0", "false", "off"}:
            bold_runs.append(run_text)

for check in checks:
    kind, wanted = check.split("=", 1)
    values = paragraphs if kind == "text" else bold_runs
    if wanted not in values:
        rendered = " | ".join(paragraphs)
        raise SystemExit(f"missing {kind} {wanted!r}; paragraphs: {rendered}")

print(
    f"BXTEST PASS wps-docx archive={archive.rsplit('/', 1)[-1]} "
    f"members={len(names)} paragraphs={len(paragraphs)}"
)
for text in paragraphs:
    print(f"BXCONTENT {text}")
for text in bold_runs:
    print(f"BXFORMAT bold {text}")
PY
