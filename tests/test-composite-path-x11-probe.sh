#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/composite-path-x11-probe.json"
grep -F 'RedirectSubwindows' \
    "$repo_dir/examples/composite-path-x11-probe/composite-path-x11-probe.c" >/dev/null
grep -F 'NameWindowPixmap' \
    "$repo_dir/examples/composite-path-x11-probe/composite-path-x11-probe.c" >/dev/null
grep -F 'DamageNotify' \
    "$repo_dir/examples/composite-path-x11-probe/composite-path-x11-probe.c" >/dev/null
grep -F 'GetOverlayWindow' \
    "$repo_dir/examples/composite-path-x11-probe/README.md" >/dev/null || \
grep -F 'overlay' \
    "$repo_dir/examples/composite-path-x11-probe/composite-path-x11-probe.c" >/dev/null
grep -F 'XGrabServer' \
    "$repo_dir/examples/composite-path-x11-probe/composite-path-x11-probe.c" >/dev/null
grep -F 'applyRedirectStorage' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XComposite.java" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/composite-path-x11-probe/composite-path-x11-probe.c" >/dev/null; then
    echo "composite-path probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=6 failed=0' \
    "$repo_dir/examples/composite-path-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/composite-path-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/composite-path-x11-probe/install-and-run.sh" >/dev/null; then
    echo "composite-path probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/composite-path-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/composite-path-x11-probe/install-and-run.sh"
echo "composite-path probe is a two-connection compositor client: PASS"
