#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/desktop-session.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/desktop-session/install-and-run.sh" >/dev/null; then
    echo "desktop-session must not replace the shared seed" >&2
    exit 1
fi
echo "desktop session profile launches two package apps: PASS"
