#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/firefox-esr-online.json"
grep -F 'https://example.com/' \
    "$repo_dir/profiles/firefox-esr-online.json" >/dev/null
grep -F 'nss-ckbi-probe' \
    "$repo_dir/examples/firefox-online/run-online.sh" >/dev/null
grep -F 'firefox-esr-online' \
    "$repo_dir/examples/firefox-online/run-online.sh" >/dev/null
grep -F 'cert9.db' \
    "$repo_dir/examples/firefox-online/run-online.sh" >/dev/null
grep -F 'firefox-online-cold' \
    "$repo_dir/examples/firefox-online/run-online.sh" >/dev/null
grep -F 'firefox-example-page' \
    "$repo_dir/examples/firefox-online/assert-example-page.py" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/firefox-online/run-online.sh" >/dev/null; then
    echo "firefox-online must not replace the shared seed" >&2
    exit 1
fi
echo "firefox online runner covers NSS, example.com and cold start: PASS"
