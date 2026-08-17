#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if grep -E 'libvlc_media_player_play|bionicx-motion-audio.avi' \
        "$repo_dir/examples/vlc-plugin-probe/vlc-plugin-probe.c" >/dev/null; then
    echo "vlc-plugin-probe must not demux an AVI fixture" >&2
    exit 1
fi
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/vlc-plugin-probe/install-and-run.sh" >/dev/null; then
    echo "vlc-plugin-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/vlc-plugin-probe/install-and-run.sh"
echo "vlc plugin probe covers libvlc + xcb plugin load: PASS"
