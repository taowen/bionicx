#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )
package="io.taowen.bx"
device_fonts="files/apps/chrome/usr/share/fonts/bionicx"
device_config="files/apps/chrome/etc/fonts/fonts.conf"
device_cache="files/homes/chrome/.cache/fontconfig-bionicx-v1"

command -v fc-match >/dev/null || {
    echo "fc-match is required to locate Liberation fonts" >&2
    exit 1
}

families=(
    "Liberation Sans:style=Regular"
    "Liberation Sans:style=Bold"
    "Liberation Serif:style=Regular"
    "Liberation Serif:style=Bold"
    "Liberation Mono:style=Regular"
    "Liberation Mono:style=Bold"
)
fonts=()
for family in "${families[@]}"; do
    font="$(fc-match -f '%{file}' "$family")"
    [[ -f "$font" ]] || {
        echo "missing host font for $family: install Liberation fonts" >&2
        exit 1
    }
    fonts+=("$font")
done

"${adb[@]}" shell run-as "$package" mkdir -p "$device_fonts" \
    files/apps/chrome/etc/fonts
for font in "${fonts[@]}"; do
    name="$(basename "$font")"
    temporary="/data/local/tmp/bionicx-chrome-$name"
    "${adb[@]}" push "$font" "$temporary" >/dev/null
    "${adb[@]}" shell run-as "$package" cp "$temporary" "$device_fonts/$name"
    "${adb[@]}" shell rm "$temporary"
done

temporary="/data/local/tmp/bionicx-chrome-fonts.conf"
"${adb[@]}" push "$repo_dir/examples/chrome/fonts.conf" "$temporary" >/dev/null
"${adb[@]}" shell run-as "$package" cp "$temporary" "$device_config"
"${adb[@]}" shell rm "$temporary"
"${adb[@]}" shell run-as "$package" rm -rf "$device_cache"

installed="$("${adb[@]}" shell run-as "$package" \
    find "$device_fonts" -maxdepth 1 -type f -name '*.ttf' | tr -d '\r' | wc -l)"
[[ "$installed" -eq "${#fonts[@]}" ]] || {
    echo "device verification failed: expected ${#fonts[@]} fonts, found $installed" >&2
    exit 1
}
echo "installed and verified ${#fonts[@]} Chrome fallback fonts"
