#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/map-request-x11-probe.json"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/map-request-x11-probe/install-and-run.sh" >/dev/null; then
    echo "map-request probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/map-request-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/map-request-x11-probe/install-and-run.sh"
echo "map-request probe is a two-connection X11 client: PASS"
