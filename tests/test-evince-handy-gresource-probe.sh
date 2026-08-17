#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/evince-handy-gresource-probe.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/evince-handy-gresource-probe/install-and-run.sh" \
        >/dev/null; then
    echo "evince-handy-gresource-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/evince-handy-gresource-probe/install-and-run.sh"
echo "evince handy gresource probe covers libhandy theme CSS: PASS"
