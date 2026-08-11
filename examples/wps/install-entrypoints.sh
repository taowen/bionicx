#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
serial="${ANDROID_SERIAL:-}"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )
package="io.taowen.bx"
office="files/apps/wps-office/opt/kingsoft/wps-office/office6"
interpreter="/data/data/io.taowen.bx/files/rootfs/usr/lib/ld-linux-aarch64.so.1"
work_dir="$(mktemp -d)"
trap 'rm -rf "$work_dir"' EXIT

command -v patchelf >/dev/null || {
    echo "patchelf is required to prepare direct-mode WPS entrypoints" >&2
    exit 1
}

for entrypoint in wps et wpp wpspdf; do
    source_file="$work_dir/$entrypoint"
    "${adb[@]}" exec-out run-as "$package" cat "$office/$entrypoint" \
        > "$source_file"
    [[ -s "$source_file" ]] || {
        echo "missing installed WPS entrypoint: $office/$entrypoint" >&2
        exit 1
    }
    before="$(patchelf --print-interpreter "$source_file")"
    if [[ "$before" != "$interpreter" ]]; then
        patchelf --set-interpreter "$interpreter" "$source_file"
        temporary="/data/local/tmp/bionicx-$entrypoint-$$"
        "${adb[@]}" push "$source_file" "$temporary" >/dev/null
        "${adb[@]}" shell run-as "$package" cp "$temporary" \
            "$office/$entrypoint"
        "${adb[@]}" shell run-as "$package" chmod 700 \
            "$office/$entrypoint"
        "${adb[@]}" shell rm "$temporary"
    fi
    after="$(patchelf --print-interpreter "$source_file")"
    [[ "$after" == "$interpreter" ]] || {
        echo "failed to relocate $entrypoint interpreter: $after" >&2
        exit 1
    }
    printf 'BXELF entry=%s before=%s after=%s\n' \
        "$entrypoint" "$before" "$after"
done

# WPS 11.1.0.11720's Presentation serializer builds a relative
# "??/PreXXXXXXXX" mkstemp template under LANG=C. Keep this proprietary-client
# assumption in its private deployment rather than changing mkstemp semantics
# for every glibc application. Quoting prevents the remote Android shell from
# expanding the literal question marks as a glob.
temporary_dir="$office/??"
"${adb[@]}" shell "run-as '$package' mkdir -p '$temporary_dir'"
"${adb[@]}" shell "run-as '$package' chmod 700 '$temporary_dir'"
temporary_mode="$("${adb[@]}" shell \
    "run-as '$package' stat -c %a '$temporary_dir'" | tr -d '\r')"
[[ "$temporary_mode" == "700" ]] || {
    echo "wrong WPS Presentation temporary-directory mode: $temporary_mode" >&2
    exit 1
}
printf 'BXTEMP path=%s mode=%s\n' "$temporary_dir" "$temporary_mode"

builder_image="$("$repo_dir/tools/ensure-glibc-builder.sh")"
podman run --rm --userns=keep-id \
    --volume "$work_dir:/output:Z" "$builder_image" sh -c \
    'cp -L /usr/lib/aarch64-linux-gnu/libXtst.so.6 /output/libXtst.so.6'
readelf -V "$work_dir/libXtst.so.6" | grep -Fq 'GLIBC_2.17'
temporary="/data/local/tmp/bionicx-libXtst-$$.so.6"
"${adb[@]}" push "$work_dir/libXtst.so.6" "$temporary" >/dev/null
"${adb[@]}" shell run-as "$package" cp "$temporary" \
    files/rootfs/usr/lib/libXtst.so.6
"${adb[@]}" shell rm "$temporary"
host_hash="$(sha256sum "$work_dir/libXtst.so.6" | cut -d' ' -f1)"
device_hash="$("${adb[@]}" exec-out run-as "$package" \
    sha256sum files/rootfs/usr/lib/libXtst.so.6 | cut -d' ' -f1)"
[[ "$device_hash" == "$host_hash" ]] || {
    echo "device libXtst hash mismatch" >&2
    exit 1
}
printf 'BXELF library=libXtst.so.6 sha256=%s\n' "$host_hash"
