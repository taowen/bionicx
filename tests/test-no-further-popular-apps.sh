#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
note="$repo_dir/docs/diagnostics/2026-08-14-no-further-popular-apps.md"
test -s "$note"
if grep -E '^(blender|kdenlive|transmission|audacious|hexchat|pidgin|gedit)\b' \
        "$repo_dir/packages/trixie-popular.txt"; then
    echo "trixie-popular must not grow per-app-only names" >&2
    exit 1
fi
echo "further popular apps stay off the declaration unless they extend a contract: PASS"
