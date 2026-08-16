#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/xfce-session.json"
grep -F 'usr/bin/thunar' "$repo_dir/profiles/xfce-session.json" >/dev/null
grep -F 'usr/bin/mousepad' "$repo_dir/profiles/xfce-session.json" >/dev/null
grep -F 'XDG_CURRENT_DESKTOP' "$repo_dir/profiles/xfce-session.json" >/dev/null
grep -F 'xfce-session-launch' "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F 'xfce-wm' "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F 'xfce-compositor' "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F 'xfce-panel' "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F 'session-switch-thunar' "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F 'session-resize-thunar' "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F 'thunar-geometry.xml' \
    "$repo_dir/examples/xfce-session/install-and-run.sh" >/dev/null
grep -F 'last-window-maximized' \
    "$repo_dir/examples/xfce-session/thunar-geometry.xml" >/dev/null
grep -F 'value="800"' \
    "$repo_dir/examples/xfce-session/thunar-geometry.xml" >/dev/null
grep -F '_NET_SUPPORTING_WM_CHECK' \
    "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F -- '--compositor=on' "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F -- '--vblank=off' "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null
grep -F 'REDIRECT_SUBWINDOWS' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XComposite.java" >/dev/null
grep -F 'DISPLAY=:0' \
    "$repo_dir/android/app/src/main/java/com/winlator/xenvironment/components/DBusComponent.java" \
    >/dev/null
grep -F 'GCONV_PATH=' \
    "$repo_dir/android/app/src/main/java/com/winlator/BionicXActivity.java" \
    >/dev/null
grep -F 'new XResExtension' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/XServer.java" \
    >/dev/null
grep -F 'grabOwner' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/WindowManager.java" >/dev/null
if grep -F 'window.isDock()' \
        "$repo_dir/android/app/src/main/java/com/winlator/xserver/WindowManager.java" >/dev/null; then
    echo "MapWindow must not special-case TYPE_DOCK" >&2
    exit 1
fi
if grep -F 'force_map_tree' \
        "$repo_dir/examples/xfce-session/xfce-session.c" >/dev/null; then
    echo "xfce-session must not override-redirect map the panel" >&2
    exit 1
fi
grep -F 'xfwm4' "$repo_dir/packages/trixie-popular.txt" >/dev/null
grep -F 'xfce4-panel' "$repo_dir/packages/trixie-popular.txt" >/dev/null
grep -F 'xfdesktop4' "$repo_dir/packages/trixie-popular.txt" >/dev/null
grep -F 'passed=10 failed=0' \
    "$repo_dir/examples/xfce-session/install-and-run.sh" >/dev/null
grep -F 'does not support the XSync extension' \
    "$repo_dir/examples/xfce-session/install-and-run.sh" >/dev/null
grep -F 'Unsupported keyboard modifier' \
    "$repo_dir/examples/xfce-session/install-and-run.sh" >/dev/null
grep -F 'XRandR initialization error' \
    "$repo_dir/examples/xfce-session/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/xfce-session/install-and-run.sh" >/dev/null; then
    echo "xfce-session must not replace the shared seed" >&2
    exit 1
fi
if grep -F 'xfce4-session' "$repo_dir/profiles/xfce-session.json" >/dev/null; then
    echo "xfce-session must not require systemd xfce4-session" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/xfce-session/install-and-run.sh"
echo "xfce session profile launches xfwm4 plus two package apps: PASS"
