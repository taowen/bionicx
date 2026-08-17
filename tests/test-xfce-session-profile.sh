#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" "$repo_dir/profiles/xfce-session.json"
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
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/xfce-session/install-and-run.sh" >/dev/null; then
    echo "xfce-session must not replace the shared seed" >&2
    exit 1
fi
if grep -F 'xfce4-session' "$repo_dir/profiles/xfce-session.json" >/dev/null; then
    echo "xfce-session must not require systemd xfce4-session" >&2
    exit 1
fi
if ! grep -F 'IconThemeName' \
        "$repo_dir/examples/xfce-session/xsettings.xml" >/dev/null; then
    echo "xfce-session must pin Adwaita instead of the Tango stub" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/xfce-session/install-and-run.sh"
echo "xfce session profile launches xfwm4 plus two package apps: PASS"
