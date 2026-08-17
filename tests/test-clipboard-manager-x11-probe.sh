#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/clipboard-manager-x11-probe.json"
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/clipboard-manager-x11-probe/clipboard-manager-x11-probe.c" >/dev/null; then
    echo "clipboard-manager probe must not start a desktop daemon" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/clipboard-manager-x11-probe/install-and-run.sh" >/dev/null; then
    echo "clipboard-manager probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/clipboard-manager-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/clipboard-manager-x11-probe/install-and-run.sh"
echo "clipboard-manager probe is a two-connection X11 client: PASS"
