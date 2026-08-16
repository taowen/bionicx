#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/xrender-x11-probe.json"
grep -F 'XRenderComposite' \
    "$repo_dir/examples/xrender-x11-probe/xrender-x11-probe.c" >/dev/null
grep -F 'PictOpSaturate' \
    "$repo_dir/examples/xrender-x11-probe/xrender-x11-probe.c" >/dev/null
grep -F 'XRenderCreateLinearGradient' \
    "$repo_dir/examples/xrender-x11-probe/xrender-x11-probe.c" >/dev/null
grep -F 'CPClipMask' \
    "$repo_dir/examples/xrender-x11-probe/xrender-x11-probe.c" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/xrender-x11-probe/xrender-x11-probe.c" >/dev/null; then
    echo "Render probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=1 failed=0' \
    "$repo_dir/examples/xrender-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/xrender-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/xrender-x11-probe/install-and-run.sh" >/dev/null; then
    echo "Render probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/xrender-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/xrender-x11-probe/install-and-run.sh"
echo "Render probe is a libXrender client: PASS"
