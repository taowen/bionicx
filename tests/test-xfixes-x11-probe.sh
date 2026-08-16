#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/xfixes-x11-probe.json"
grep -F 'XFixesGetCursorImage' \
    "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null
grep -F 'XFixesChangeCursor' \
    "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null
grep -F 'XFixesHideCursor' \
    "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null
grep -F 'XFixesChangeSaveSet' \
    "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null
grep -F 'XFixesIntersectRegion' \
    "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null
grep -F 'XFixesTranslateRegion' \
    "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null
grep -F 'XFixesCreateRegionFromWindow' \
    "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null
grep -F 'XFixesSetPictureClipRegion' \
    "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null
grep -F 'SET_PICTURE_CLIP_REGION' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XFixesExtension.java" \
    >/dev/null
grep -F 'INTERSECT_REGION' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XFixesExtension.java" >/dev/null
grep -F 'CREATE_REGION_FROM_WINDOW' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XFixesExtension.java" >/dev/null
grep -F 'GET_CURSOR_IMAGE_AND_NAME' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XFixesExtension.java" >/dev/null
grep -F 'HIDE_CURSOR' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/XFixesExtension.java" >/dev/null
if grep -E 'xfsettingsd|xfwm4|icewm' \
        "$repo_dir/examples/xfixes-x11-probe/xfixes-x11-probe.c" >/dev/null; then
    echo "xfixes probe must not start a desktop daemon" >&2
    exit 1
fi
grep -F 'passed=10 failed=0' \
    "$repo_dir/examples/xfixes-x11-probe/install-and-run.sh" >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/xfixes-x11-probe/build-bundle.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/xfixes-x11-probe/install-and-run.sh" >/dev/null; then
    echo "xfixes probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/xfixes-x11-probe/build-bundle.sh" \
    "$repo_dir/examples/xfixes-x11-probe/install-and-run.sh"
echo "xfixes probe is a libXfixes client: PASS"
