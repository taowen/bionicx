#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/get-property-x11-probe.json"
if grep -E 'xfwm4|icewm' \
        "$repo_dir/examples/get-property-x11-probe/get-property-x11-probe.c" >/dev/null; then
    echo "get-property probe must not start a window manager" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/get-property-x11-probe/install-and-run.sh" >/dev/null; then
    echo "get-property probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/get-property-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/get-property-x11-probe/install-and-run.sh"
echo "get-property probe is a libX11 client: PASS"
