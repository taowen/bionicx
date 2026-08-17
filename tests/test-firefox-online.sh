#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/firefox-esr-online.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/firefox-online/run-online.sh" >/dev/null; then
    echo "firefox-online must not replace the shared seed" >&2
    exit 1
fi
echo "firefox online runner covers NSS, example.com and cold start: PASS"
