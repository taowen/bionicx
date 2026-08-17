#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/gdk-pixbuf-svg-probe.json"
if grep -E 'xfsettingsd|xfwm4|icewm|thunar|mousepad' \
        "$repo_dir/examples/gdk-pixbuf-svg-probe/gdk-pixbuf-svg-probe.c" >/dev/null; then
    echo "gdk-pixbuf-svg probe must not start a desktop daemon" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/gdk-pixbuf-svg-probe/install-and-run.sh" >/dev/null; then
    echo "gdk-pixbuf-svg probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/gdk-pixbuf-svg-probe/build-bundle.sh" \
    "$repo_dir/examples/gdk-pixbuf-svg-probe/install-and-run.sh"
echo "gdk-pixbuf SVG probe is a loader/pixbuf client: PASS"
