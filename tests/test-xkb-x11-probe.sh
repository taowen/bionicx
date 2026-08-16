#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/xkb-x11-probe.json"
grep -F 'XkbGetMap' \
    "$repo_dir/examples/xkb-x11-probe/xkb-x11-probe.c" >/dev/null
grep -F 'xkb_x11_keymap_new_from_device' \
    "$repo_dir/examples/xkb-x11-probe/xkb-x11-probe.c" >/dev/null
grep -F 'XTestFakeKeyEvent' \
    "$repo_dir/examples/xkb-x11-probe/xkb-x11-probe.c" >/dev/null
grep -F 'XkbStateNotifyMask' \
    "$repo_dir/examples/xkb-x11-probe/xkb-x11-probe.c" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/xkb-x11-probe/xkb-x11-probe.c" >/dev/null; then
    echo "XKB probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=3 failed=0' \
    "$repo_dir/examples/xkb-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/xkb-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/xkb-x11-probe/install-and-run.sh" >/dev/null; then
    echo "XKB probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/xkb-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/xkb-x11-probe/install-and-run.sh"
echo "XKB probe is a libxkbcommon-x11 client: PASS"
