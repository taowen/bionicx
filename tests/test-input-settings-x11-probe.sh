#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/input-settings-x11-probe.json"
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/input-settings-x11-probe/input-settings-x11-probe.c" >/dev/null; then
    echo "input-settings probe must not start a desktop daemon" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/input-settings-x11-probe/install-and-run.sh" >/dev/null; then
    echo "input-settings probe must not replace the shared seed" >&2
    exit 1
fi
test ! -e "$repo_dir/examples/pointer-control-x11-probe/pointer-control-x11-probe.c"
test ! -e "$repo_dir/examples/xkb-set-controls-x11-probe/xkb-set-controls-x11-probe.c"
chmod +x "$repo_dir/examples/input-settings-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/input-settings-x11-probe/install-and-run.sh"
echo "input-settings probe is a libX11 client: PASS"
