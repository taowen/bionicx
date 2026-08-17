#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/gtk-gdk-grab-probe.json"
if grep -E 'xfsettingsd|xfwm4|icewm|thunar|mousepad' \
        "$repo_dir/examples/gtk-gdk-grab-probe/gtk-gdk-grab-probe.c" >/dev/null; then
    echo "gtk-gdk-grab probe must not start a desktop daemon" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/gtk-gdk-grab-probe/install-and-run.sh" >/dev/null; then
    echo "gtk-gdk-grab probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/gtk-gdk-grab-probe/build-bundle.sh" \
    "$repo_dir/examples/gtk-gdk-grab-probe/install-and-run.sh"
echo "gtk-gdk-grab probe is a GDK filter grab client: PASS"
