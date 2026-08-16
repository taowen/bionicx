#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
grep -F 'libvlc.so.5' \
    "$repo_dir/examples/vlc-plugin-probe/vlc-plugin-probe.c" >/dev/null
grep -F 'libxcb_x11_plugin.so' \
    "$repo_dir/examples/vlc-plugin-probe/vlc-plugin-probe.c" >/dev/null
grep -F 'libvlc_new' \
    "$repo_dir/examples/vlc-plugin-probe/vlc-plugin-probe.c" >/dev/null
grep -F 'VLC_PLUGIN_PATH' \
    "$repo_dir/examples/vlc-plugin-probe/install-and-run.sh" >/dev/null
grep -F 'failed=0' \
    "$repo_dir/examples/vlc-plugin-probe/install-and-run.sh" >/dev/null
if grep -E 'libvlc_media_player_play|bionicx-motion-audio.avi' \
        "$repo_dir/examples/vlc-plugin-probe/vlc-plugin-probe.c" >/dev/null; then
    echo "vlc-plugin-probe must not demux an AVI fixture" >&2
    exit 1
fi
grep -F 'bionicx-motion-audio.avi' "$repo_dir/profiles/vlc.json" >/dev/null
grep -F 'pulseaudio' "$repo_dir/profiles/vlc.json" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/vlc-plugin-probe/install-and-run.sh" >/dev/null; then
    echo "vlc-plugin-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/vlc-plugin-probe/install-and-run.sh"
echo "vlc plugin probe covers libvlc + xcb plugin load: PASS"
