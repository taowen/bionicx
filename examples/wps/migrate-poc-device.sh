#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
serial="${ANDROID_SERIAL:-01408BH601027129}"
apk="${BIONICX_APK:-$repo_dir/build/bionicx-debug.apk}"
relocator="$repo_dir/build/bionicx-relocate"
old_package="com.winlator"
new_package="io.taowen.bx"
old_prefix="/data/data/$old_package"
new_prefix="/data/data/$new_package"
adb=("$adb_bin" -s "$serial")

[[ -f "$apk" ]] || "$repo_dir/tools/build.sh"
if [[ ! -x "$relocator" ]]; then
    ndk_root="${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/29.0.14206865}"
    "$ndk_root/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android28-clang" \
        -Oz -Wall -Wextra -Werror "$repo_dir/native/tools/bionicx-relocate.c" \
        -o "$relocator"
fi
"${adb[@]}" install -r "$apk"
"${adb[@]}" shell "xsu -c 'test -x $old_prefix/files/wps-root/opt/kingsoft/wps-office/office6/wps'"

uid="$("${adb[@]}" shell "xsu -c 'stat -c %u $new_prefix'" | tr -d '\r')"
"${adb[@]}" shell "xsu -c '
    mkdir -p $new_prefix/files/apps $new_prefix/files/homes $new_prefix/files/profiles
    mkdir -p $new_prefix/files/rootfs
    cp -a $old_prefix/files/rootfs/. $new_prefix/files/rootfs/
    test -d $new_prefix/files/apps/wps-office || cp -a $old_prefix/files/wps-root $new_prefix/files/apps/wps-office
    test -d $new_prefix/files/homes/wps-office || cp -a $old_prefix/files/wps-root/home $new_prefix/files/homes/wps-office
    chown -R $uid:$uid $new_prefix/files
'"

temporary="$(mktemp -d)"
for relative in \
    rootfs/usr/lib/libxcb.so.1 \
    apps/wps-office/opt/kingsoft/wps-office/office6/wps; do
    basename_safe="$(printf '%s' "$relative" | tr '/' '_')"
    device_tmp="/data/local/tmp/bionicx-$basename_safe"
    "${adb[@]}" shell "xsu -c 'cp $new_prefix/files/$relative $device_tmp && chmod 644 $device_tmp'"
    "${adb[@]}" pull "$device_tmp" "$temporary/$basename_safe" >/dev/null
done

python3 - "$temporary" "$old_prefix" "$new_prefix" <<'PY'
import sys
from pathlib import Path
root, old, new = Path(sys.argv[1]), sys.argv[2].encode(), sys.argv[3].encode()
assert len(old) == len(new), (old, new)
for path in root.iterdir():
    data = path.read_bytes()
    if old not in data:
        raise SystemExit(f"old prefix not found in {path}")
    path.write_bytes(data.replace(old, new))
PY

for relative in \
    rootfs/usr/lib/libxcb.so.1 \
    apps/wps-office/opt/kingsoft/wps-office/office6/wps; do
    basename_safe="$(printf '%s' "$relative" | tr '/' '_')"
    device_tmp="/data/local/tmp/bionicx-$basename_safe-patched"
    "${adb[@]}" push "$temporary/$basename_safe" "$device_tmp" >/dev/null
    "${adb[@]}" shell "xsu -c 'cp $device_tmp $new_prefix/files/$relative && chown $uid:$uid $new_prefix/files/$relative'"
done
"${adb[@]}" shell "xsu -c 'chmod 700 $new_prefix/files/apps/wps-office/opt/kingsoft/wps-office/office6/wps'"
"${adb[@]}" shell "xsu -c 'test -e $new_prefix/files/wps-root || ln -s apps/wps-office $new_prefix/files/wps-root'"

# Relocate every fixed-width prefix in the copied runtime, including ELF
# RUNPATH strings and X11 locale/font configuration, without changing length.
relocator_tmp="/data/local/tmp/bionicx-relocate"
"${adb[@]}" push "$relocator" "$relocator_tmp" >/dev/null
"${adb[@]}" shell "xsu -c 'chmod 700 $relocator_tmp && $relocator_tmp $new_prefix/files $old_prefix $new_prefix'"

profile_tmp="/data/local/tmp/bionicx-wps-profile.json"
"${adb[@]}" push "$repo_dir/profiles/wps-office.json" "$profile_tmp" >/dev/null
"${adb[@]}" shell "xsu -c 'cp $profile_tmp $new_prefix/files/profiles/active.json && chown $uid:$uid $new_prefix/files/profiles/active.json'"
"${adb[@]}" shell am force-stop "$new_package"
"${adb[@]}" shell am start -W -n "$new_package/com.winlator.BionicXActivity"
