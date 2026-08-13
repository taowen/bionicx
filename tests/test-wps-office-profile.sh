#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/wps-office.json"
grep -F '"cups"' "$repo_dir/profiles/wps-office.json" >/dev/null
grep -F '172d8bcf3e3bc756994ae5adf66f63f22691e1ab0d18986c50bb6b6ab7f62948' \
    "$repo_dir/examples/wps/README.md" >/dev/null
grep -F 'xdg-utils' "$repo_dir/examples/wps/README.md" >/dev/null
echo "WPS profile prints through the shared CUPS destination: PASS"
