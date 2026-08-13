#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
grep -F 'XTestFakeKeyEvent' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'XTestFakeButtonEvent' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'XTestFakeButtonEvent(d, 1, True, 40)' \
    "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'click-frac' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'click-window' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'click-window "System Check"' \
    "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
grep -F "printf '%q'" "$repo_dir/examples/wps/send-key.sh" >/dev/null
grep -F 'files/homes/wps-office/Desktop' \
    "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
grep -F 'ctrl-s' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'ctrl-c' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'ctrl-v' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'XK_F5' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'type_ascii' "$repo_dir/examples/wps/x11-send-key.c" >/dev/null
grep -F 'verify-docx.sh' "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
grep -F 'verify-xlsx.sh' "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
grep -F 'verify-pptx.sh' "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
grep -F 'BionicX_WF_' "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
grep -F 'ctrl-s' "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
grep -F 'f5' "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
grep -F 'ctrl-p' "$repo_dir/examples/wps/run-workflows.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/wps/run-workflows.sh" >/dev/null; then
    echo "wps workflows must not replace the shared seed" >&2
    exit 1
fi
echo "wps workflow runner covers save, clipboard, print and slideshow: PASS"
