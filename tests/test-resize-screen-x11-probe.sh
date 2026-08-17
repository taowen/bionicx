#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/resize-screen-x11-probe.json"
if grep -E 'xfsettingsd|xfwm4|icewm|thunar|mousepad' \
        "$repo_dir/examples/resize-screen-x11-probe/resize-screen-x11-probe.c" >/dev/null; then
    echo "resize-screen probe must not start a desktop daemon" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/resize-screen-x11-probe/install-and-run.sh" >/dev/null; then
    echo "resize-screen probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/resize-screen-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/resize-screen-x11-probe/install-and-run.sh" \
    "$repo_dir/examples/resize-screen-x11-probe/assert-screenshot.py"
echo "resize-screen probe is a two-connection X11 client: PASS"
