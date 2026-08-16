#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/named-pixmap-resize-x11-probe.json"
grep -F 'XResizeWindow' \
    "$repo_dir/examples/named-pixmap-resize-x11-probe/named-pixmap-resize-x11-probe.c" >/dev/null
grep -F 'NameWindowPixmap' \
    "$repo_dir/examples/named-pixmap-resize-x11-probe/README.md" >/dev/null
grep -F 'named-stale-after-resize' \
    "$repo_dir/examples/named-pixmap-resize-x11-probe/named-pixmap-resize-x11-probe.c" >/dev/null
grep -F 'XGrabServer' \
    "$repo_dir/examples/named-pixmap-resize-x11-probe/named-pixmap-resize-x11-probe.c" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/named-pixmap-resize-x11-probe/named-pixmap-resize-x11-probe.c" >/dev/null; then
    echo "named-pixmap-resize probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=7 failed=0' \
    "$repo_dir/examples/named-pixmap-resize-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/named-pixmap-resize-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/named-pixmap-resize-x11-probe/install-and-run.sh" >/dev/null; then
    echo "named-pixmap-resize probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/named-pixmap-resize-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/named-pixmap-resize-x11-probe/install-and-run.sh"
echo "named-pixmap-resize probe is a two-connection X11 client: PASS"
