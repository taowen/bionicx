#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/compositor-x11-probe.json"
if awk '/public void forceUpdate/,/^    }/' \
        "$repo_dir/android/app/src/main/java/com/winlator/xserver/Drawable.java" \
        | grep -F 'offscreenStorage' >/dev/null; then
    echo "offscreen drawables must still mark textures dirty" >&2
    exit 1
fi
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/compositor-x11-probe/compositor-x11-probe.c" >/dev/null; then
    echo "compositor probe must not start a desktop daemon" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/compositor-x11-probe/install-and-run.sh" >/dev/null; then
    echo "compositor probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/compositor-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/compositor-x11-probe/install-and-run.sh"
echo "compositor probe is a two-connection xfwm4-shaped client: PASS"
