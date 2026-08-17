#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runner="$repo_dir/examples/popular-workflows/run.sh"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/krita.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/krita-export.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/qbittorrent.json"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/keepassxc.json"
if grep -F -- '--runtime-root' "$runner" >/dev/null; then
    echo "popular-workflows must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$runner"
echo "popular durable workflows cover probes, persist and export: PASS"
