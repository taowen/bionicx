#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
grep -F 'Liberation Sans' \
    "$repo_dir/examples/wps-font-probe/wps-font-probe.c" >/dev/null
grep -F 'font-family-liberation-sans' \
    "$repo_dir/examples/wps-font-probe/wps-font-probe.c" >/dev/null
grep -F 'font-family-calibri' \
    "$repo_dir/examples/wps-font-probe/wps-font-probe.c" >/dev/null
grep -F '0x2211' "$repo_dir/examples/wps-font-probe/wps-font-probe.c" >/dev/null
grep -F 'LiberationSans' \
    "$repo_dir/examples/wps-font-probe/wps-font-probe.c" >/dev/null
grep -F '50-bionicx-liberation-aliases.conf' \
    "$repo_dir/examples/wps-font-probe/install-and-run.sh" >/dev/null
grep -F 'passed=6 failed=0' \
    "$repo_dir/examples/wps-font-probe/install-and-run.sh" >/dev/null
grep -F 'Calibri' \
    "$repo_dir/examples/wps-font-probe/30-bionicx-liberation-aliases.conf" \
    >/dev/null
grep -F 'Liberation Sans' \
    "$repo_dir/examples/wps-font-probe/30-bionicx-liberation-aliases.conf" \
    >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/wps-font-probe/install-and-run.sh" >/dev/null; then
    echo "wps-font-probe must not replace the shared seed" >&2
    exit 1
fi
grep -F -- '--set-interpreter' \
    "$repo_dir/examples/wps-font-probe/install-and-run.sh" >/dev/null
echo "wps font probe checks Liberation coverage before MS aliases: PASS"
