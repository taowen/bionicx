#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 DOCUMENT.xlsx CELL=VALUE ... [--formula CELL=FORMULA ...]" >&2
}

[[ $# -ge 2 ]] || { usage; exit 2; }
document="$1"
shift
[[ "$document" =~ ^[A-Za-z0-9._-]+\.xlsx$ ]] || {
    echo "document must be a safe .xlsx basename: $document" >&2
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
    echo "empty or missing device workbook: $device_path" >&2
    exit 1
}

checks=()
while [[ $# -gt 0 ]]; do
    if [[ "$1" == "--formula" ]]; then
        [[ $# -ge 2 && "$2" == *=* ]] || { usage; exit 2; }
        checks+=("formula=$2")
        shift 2
    elif [[ "$1" == *=* ]]; then
        checks+=("value=$1")
        shift
    else
        usage
        exit 2
    fi
done

python3 - "$archive" "${checks[@]}" <<'PY'
import sys
import xml.etree.ElementTree as ET
import zipfile

archive, *checks = sys.argv[1:]
with zipfile.ZipFile(archive) as xlsx:
    bad = xlsx.testzip()
    if bad is not None:
        raise SystemExit(f"corrupt XLSX member: {bad}")
    names = set(xlsx.namelist())
    required = {
        "[Content_Types].xml",
        "_rels/.rels",
        "xl/workbook.xml",
        "xl/worksheets/sheet1.xml",
    }
    missing = sorted(required - names)
    if missing:
        raise SystemExit(f"missing XLSX members: {', '.join(missing)}")
    root = ET.fromstring(xlsx.read("xl/worksheets/sheet1.xml"))

namespace = {"s": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}
cells = {}
for cell in root.findall(".//s:c", namespace):
    reference = cell.get("r")
    value = cell.find("s:v", namespace)
    formula = cell.find("s:f", namespace)
    cells[reference] = {
        "value": "" if value is None else value.text or "",
        "formula": "" if formula is None else formula.text or "",
    }

for check in checks:
    kind, expression = check.split("=", 1)
    reference, expected = expression.split("=", 1)
    actual = cells.get(reference, {}).get(kind)
    if actual != expected:
        raise SystemExit(
            f"wrong {kind} for {reference}: expected {expected!r}, got {actual!r}"
        )

print(
    f"BXTEST PASS wps-xlsx archive={archive.rsplit('/', 1)[-1]} "
    f"members={len(names)} cells={len(cells)}"
)
for reference in sorted(cells):
    value = cells[reference]["value"]
    formula = cells[reference]["formula"]
    print(f"BXCELL {reference} value={value} formula={formula}")
PY
