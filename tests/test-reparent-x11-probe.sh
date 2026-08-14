#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/reparent-x11-probe.json"
grep -F 'XReparentWindow' \
    "$repo_dir/examples/reparent-x11-probe/reparent-x11-probe.c" >/dev/null
grep -F 'from_configure' \
    "$repo_dir/examples/reparent-x11-probe/reparent-x11-probe.c" >/dev/null
grep -F '_NET_WM_WINDOW_TYPE_DOCK' \
    "$repo_dir/examples/reparent-x11-probe/reparent-x11-probe.c" >/dev/null
grep -F 'GrabServer' \
    "$repo_dir/examples/reparent-x11-probe/reparent-x11-probe.c" >/dev/null
if grep -E 'xfwm4|icewm' \
        "$repo_dir/examples/reparent-x11-probe/reparent-x11-probe.c" >/dev/null; then
    echo "reparent probe must not start a window manager" >&2
    exit 1
fi
grep -F 'passed=5 failed=0' \
    "$repo_dir/examples/reparent-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/reparent-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/reparent-x11-probe/install-and-run.sh" >/dev/null; then
    echo "reparent probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/reparent-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/reparent-x11-probe/install-and-run.sh"
echo "reparent probe is a two-connection X11 client: PASS"
