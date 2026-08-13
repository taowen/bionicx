#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/cups-probe.json"
grep -F 'bionicx-test' "$repo_dir/examples/cups-probe/cups-probe.c" >/dev/null
grep -F 'cupsGetDests' "$repo_dir/examples/cups-probe/cups-probe.c" >/dev/null
grep -F 'FileDevice Yes' "$repo_dir/examples/cups-probe/install-and-run.sh" >/dev/null
grep -F -- '--set-rpath' "$repo_dir/examples/cups-probe/build-bundle.sh" >/dev/null
echo "cups probe profile and destination contract: PASS"
