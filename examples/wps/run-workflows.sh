#!/usr/bin/env bash
# Drive untraced WPS Writer/Sheets/Presentation on the shared seed:
# open, edit, clipboard, save, print, slideshow, cold-reopen.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
package_id=io.taowen.bx
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
evidence="${BIONICX_EVIDENCE:-$repo_dir/evidence/rebuild-2026-08-14}"
marker="${WPS_WF_MARKER:-BionicX_WF_20260814}"
mkdir -p "$evidence"

send() {
    "$repo_dir/examples/wps/send-key.sh" "$@"
}

screenshot() {
    local name="$1"
    "${adb[@]}" exec-out screencap -p > "$evidence/$name"
    printf 'BXSHOT %s bytes=%s\n' "$name" "$(stat -c %s "$evidence/$name")"
}

kill_session() {
    "${adb[@]}" shell 'kill -9 $(pidof cupsd) $(pidof bionicx-exec) $(pidof wps) $(pidof et) $(pidof wpp) $(pidof wpspdf) 2>/dev/null; true'
    "${adb[@]}" shell am force-stop io.taowen.bx
}

launch() {
    local profile="$1"
    kill_session
    "${adb[@]}" logcat -c
    "$repo_dir/tools/install-profile.sh" --serial "$serial" --profile "$profile"
    "${adb[@]}" shell am start -W \
        -n io.taowen.bx/com.winlator.BionicXActivity >/dev/null
    for _ in $(seq 1 200); do
        if "${adb[@]}" logcat -d -v brief | grep -Fq 'running untraced'; then
            break
        fi
        sleep 0.1
    done
    # Formula / guest / first-run dialogs sit on the 1920x1080 desktop.
    sleep 8
    send click 1150 620 || true   # default-app OK
    sleep 1
    send click 1680 180 || true   # guest popup close
    sleep 1
    send click 680 430 || true    # formula-check Close
    sleep 1
    send escape || true
    sleep 1
    send click 960 820 || true    # document body
    sleep 1
}

copy_home() {
    local src="$1" dest="$2"
    "${adb[@]}" shell run-as "$package_id" mkdir -p \
        files/homes/wps-office/Documents
    "${adb[@]}" shell run-as "$package_id" cp "$src" "$dest"
}

write_profile() {
    local dest="$1" id="$2" exe="$3" doc="$4" cups="$5"
    python3 - "$dest" "$id" "$exe" "$doc" "$cups" <<'PY'
import json, sys
dest, ident, exe, doc, cups = sys.argv[1:]
profile = {
    "$schema": "../schemas/profile.schema.json",
    "schemaVersion": 3,
    "id": ident,
    "name": ident,
    "display": {"dpi": 144, "socket": "filesystem"},
    "launch": {
        "executable": "${RUNTIME}/opt/kingsoft/wps-office/office6/" + exe,
        "workingDirectory": "${RUNTIME}/opt/kingsoft/wps-office/office6",
        "argv0": exe,
        "arguments": ["${HOME}/Documents/" + doc],
        "environment": {
            "DISPLAY": "${DISPLAY}",
            "HOME": "${HOME}",
            "TMPDIR": "${TMP}",
            "XDG_RUNTIME_DIR": "${TMP}/runtime",
            "GCONV_PATH": "${RUNTIME}/usr/lib/aarch64-linux-gnu/gconv",
            "LANG": "C",
            "QT_QPA_PLATFORM": "xcb",
            "QT_X11_NO_MITSHM": "1",
            "PATH": "${RUNTIME}/usr/bin:${RUNTIME}/bin:/product/bin:/system/bin",
        },
    },
}
if cups == "cups":
    profile["hostServices"] = ["cups"]
    profile["launch"]["environment"]["BIONICX_OPEN_PDF"] = (
        "${RUNTIME}/opt/kingsoft/wps-office/office6/wpspdf"
    )
json.dump(profile, open(dest, "w"), indent=2)
print(dest)
PY
}

seed_eula() {
    local home="$1"
    local conf="files/homes/$home/.config/Kingsoft/Office.conf"
    "${adb[@]}" shell run-as "$package_id" mkdir -p \
        "files/homes/$home/.config/Kingsoft"
    if "${adb[@]}" shell run-as "$package_id" grep -Fq 'AcceptedEULA=true' \
            "$conf" 2>/dev/null; then
        return 0
    fi
    tmp="/data/local/tmp/wps-accepted-eula-$$.conf"
    "${adb[@]}" push "$repo_dir/examples/wps/accepted-eula.conf" "$tmp" \
        >/dev/null
    "${adb[@]}" shell run-as "$package_id" cp "$tmp" "$conf"
    "${adb[@]}" shell rm -f "$tmp"
}

log="$evidence/wps-workflows-20260814.log"
exec > >(tee "$log") 2>&1

echo "BXINFO marker=$marker seed-check"
adb -s "$serial" shell run-as "$package_id" \
    cat files/rootfs/.bionicx-rootfs-seed-id

# --- Writer: open seed, append marker, copy/paste, save, print, cold-reopen
copy_home \
    files/homes/wps-office/Documents/BionicX-Seed-Writer-20260813.docx \
    files/homes/wps-office/Documents/BionicX-WF-Writer.docx
seed_eula wps-office
writer_profile="$repo_dir/build/wps-writer-workflow.json"
write_profile "$writer_profile" wps-office wps BionicX-WF-Writer.docx cups
launch "$writer_profile"
# Leave the formula dialog and move to the end of the document.
send end || true
send return || true
send type "$marker"
send return || true
send type "${marker}_clip"
# Select the clip line, copy, paste a duplicate.
send ctrl-a || true
sleep 0.3
send ctrl-c || true
sleep 0.3
send end || true
send return || true
send ctrl-v || true
send ctrl-s || true
sleep 3
screenshot wps-writer-edited.png
# Print the edited document to the controlled CUPS dest.
send ctrl-p || true
sleep 3
screenshot wps-writer-print-dialog.png
send return || true
sleep 4
screenshot wps-writer-printed.png

"$repo_dir/examples/wps/verify-docx.sh" BionicX-WF-Writer.docx \
    "$marker" "${marker}_clip"

kill_session
launch "$writer_profile"
screenshot wps-writer-cold-reopen.png
"$repo_dir/examples/wps/verify-docx.sh" BionicX-WF-Writer.docx \
    "$marker" "${marker}_clip"
echo "BXTEST PASS wps-writer-workflow marker=$marker"

# --- Sheets: persist the SUM workbook and add a dated numeric cell
copy_home \
    files/homes/wps-office/Documents/Book1.xlsx \
    files/homes/wps-office/Documents/BionicX-WF-Sheets.xlsx
sheets_profile="$repo_dir/build/wps-sheets-workflow.json"
write_profile "$sheets_profile" wps-office et BionicX-WF-Sheets.xlsx none
# verify-xlsx hardcodes the wps-office Documents path and the basename.
launch "$sheets_profile"
# Leave A1:A3 (12 / 30 / SUM) and type a dated value into A4.
send return || true
send return || true
send return || true
send type "20260814"
send ctrl-s || true
sleep 3
screenshot wps-sheets-edited.png
"$repo_dir/examples/wps/verify-xlsx.sh" BionicX-WF-Sheets.xlsx \
    A1=12 A2=30 A3=42 --formula 'A3=SUM(A1:A2)'
kill_session
launch "$sheets_profile"
screenshot wps-sheets-cold-reopen.png
"$repo_dir/examples/wps/verify-xlsx.sh" BionicX-WF-Sheets.xlsx \
    A1=12 A2=30 A3=42 --formula 'A3=SUM(A1:A2)'
echo "BXTEST PASS wps-sheets-workflow formula=SUM"

# --- Presentation: open saved deck, slideshow, persist
copy_home \
    files/homes/wps-office/Documents/BionicX-Presentation.pptx \
    files/homes/wps-office/Documents/BionicX-WF-Slides.pptx
slides_profile="$repo_dir/build/wps-slides-workflow.json"
write_profile "$slides_profile" wps-office wpp BionicX-WF-Slides.pptx none
launch "$slides_profile"
screenshot wps-slides-open.png
send f5 || true
sleep 3
screenshot wps-slides-slideshow.png
send escape || true
sleep 1
send ctrl-s || true
sleep 2
"$repo_dir/examples/wps/verify-pptx.sh" BionicX-WF-Slides.pptx \
    'BionicX Presentation' 'glibc and X11 on Android'
kill_session
launch "$slides_profile"
screenshot wps-slides-cold-reopen.png
"$repo_dir/examples/wps/verify-pptx.sh" BionicX-WF-Slides.pptx \
    'BionicX Presentation' 'glibc and X11 on Android'
echo "BXTEST PASS wps-slides-workflow slideshow"

# Export artifact: newest CUPS PDF spool after Writer print.
spool="$("${adb[@]}" shell run-as "$package_id" \
    ls files/run/cups/spool | tr -d '\r' | awk '/^d[0-9]+-001$/{s=$0} END{print s}')"
if [[ -n "$spool" ]]; then
    "${adb[@]}" exec-out run-as "$package_id" \
        cat "files/run/cups/spool/$spool" \
        > "$evidence/wps-writer-export.pdf"
    python3 - "$evidence/wps-writer-export.pdf" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
ok = data.startswith(b"%PDF-") and len(data) > 1000
print(f"BXTEST {'PASS' if ok else 'FAIL'} wps-writer-export bytes={len(data)} pdf={data[:5]!r}")
raise SystemExit(0 if ok else 1)
PY
fi

echo "BXSUMMARY wps-workflows writer+sheets+slides"
