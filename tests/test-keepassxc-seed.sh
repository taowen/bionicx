#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/keepassxc.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/keepassxc/seed-db.sh" >/dev/null; then
    echo "keepassxc seed-db must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/keepassxc/seed-db.sh"
echo "keepassxc app fixture seeds a key-file database: PASS"
