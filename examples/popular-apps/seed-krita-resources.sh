#!/usr/bin/env bash
# Copy Debian default Krita bundles into the user resource dir with a
# regular write (not QSaveFile). KConfig commit still cannot linkat a
# temp file on f2fs; this is enough for first-run presets.
set -euo pipefail

serial="${ANDROID_SERIAL:?ANDROID_SERIAL is required}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin" -s "$serial")
files=/data/user/0/io.taowen.bx/files
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
local_script="$(mktemp "$repo_dir/build/seed-krita.XXXXXX")"
remote_script=/data/local/tmp/bionicx-seed-krita.sh

cat >"$local_script" <<'EOF'
#!/bin/sh
set -eu
home=/data/user/0/io.taowen.bx/files/homes/krita
mkdir -p "$home/.config" "$home/.local/share/krita"
cp -a /usr/share/krita/bundles/*.bundle "$home/.local/share/krita/"
printf '%s\n' '[General]' 'HideSplashScreen=true' > "$home/.config/kritarc"
test -s "$home/.local/share/krita/Krita_4_Default_Resources.bundle"
EOF

"${adb[@]}" push "$local_script" "$remote_script" >/dev/null
"${adb[@]}" shell run-as io.taowen.bx cp "$remote_script" files/run/seed-krita.sh
"${adb[@]}" shell rm -f "$remote_script"
rm -f "$local_script"
"${adb[@]}" shell run-as io.taowen.bx \
    "$files/bin/bionicx-exec" \
    --cwd "$files/rootfs" \
    --env "LD_PRELOAD=$files/lib/libbionicx-runtime.so" \
    --env "BIONICX_ROOTFS=$files/rootfs" \
    --env "BIONICX_TMPDIR=$files/cache" \
    --env "PATH=$files/rootfs/usr/bin:$files/rootfs/bin" \
    -- "$files/rootfs/bin/sh" "$files/run/seed-krita.sh"
echo "seeded Krita default resource bundles"
