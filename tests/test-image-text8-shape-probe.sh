#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$repo_dir/tools/validate-profile.py" \
    "$repo_dir/profiles/image-text8-shape-probe.json"
grep -F 'IMAGE_TEXT8 = 76' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/ClientOpcodes.java" \
    >/dev/null
grep -F 'drawImageText8' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/requests/DrawRequests.java" \
    >/dev/null
grep -F 'class ShapeExtension' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/extensions/ShapeExtension.java" \
    >/dev/null
grep -F 'new ShapeExtension' \
    "$repo_dir/android/app/src/main/java/com/winlator/xserver/XServer.java" \
    >/dev/null
grep -F 'XDrawImageString' \
    "$repo_dir/examples/image-text8-shape-probe/image-text8-shape-probe.c" \
    >/dev/null
grep -F 'XShapeCombineRectangles' \
    "$repo_dir/examples/image-text8-shape-probe/image-text8-shape-probe.c" \
    >/dev/null
grep -F 'rootfs_payload=none' \
    "$repo_dir/examples/image-text8-shape-probe/build-bundle.sh" \
    >/dev/null
echo "image-text8/SHAPE probe and server opcodes: PASS"
