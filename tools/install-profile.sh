#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "usage: $0 --profile FILE [--app-root DIR] [--runtime-root DIR] [--serial SERIAL]" >&2
}

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
adb_bin="${ADB:-$HOME/Android/Sdk/platform-tools/adb}"
package="io.taowen.bx"
profile=""
app_root=""
runtime_root=""
serial="${ANDROID_SERIAL:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --profile) profile="$2"; shift 2 ;;
        --app-root) app_root="$2"; shift 2 ;;
        --runtime-root) runtime_root="$2"; shift 2 ;;
        --serial) serial="$2"; shift 2 ;;
        *) usage; exit 2 ;;
    esac
done
[[ -f "$profile" ]] || { usage; exit 2; }

python3 "$repo_dir/tools/validate-profile.py" "$profile"
profile_id="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["id"])' "$profile")"
if [[ -n "$app_root" ]]; then
    [[ -d "$app_root" ]] || { echo "missing app root: $app_root" >&2; exit 1; }
fi
if [[ -n "$runtime_root" ]]; then
    [[ -d "$runtime_root" ]] || { echo "missing runtime root: $runtime_root" >&2; exit 1; }
    # The seed bundle directory contains metadata plus a nested rootfs/. Passing
    # the bundle extracts INPUT-ID next to a nested Debian tree, and bxapt then
    # cannot find /usr/bin/apt-get.
    if [[ -d "$runtime_root/rootfs/usr" && ! -e "$runtime_root/usr" ]]; then
        echo "runtime-root looks like a seed bundle; use $runtime_root/rootfs" >&2
        exit 2
    fi
    if [[ ! -d "$runtime_root/usr" ]]; then
        echo "runtime-root is not a Debian rootfs (missing usr/): $runtime_root" >&2
        exit 2
    fi
fi

adb=("$adb_bin")
[[ -z "$serial" ]] || adb+=( -s "$serial" )

"${adb[@]}" shell run-as "$package" mkdir -p \
    "files/profiles" "files/apps/$profile_id" "files/rootfs"

if [[ -n "$app_root" ]]; then
    # App payloads are immutable bundles. Clear only this profile's payload so
    # removed plugins cannot survive an upgrade and mask an incomplete build.
    "${adb[@]}" shell run-as "$package" find \
        "files/apps/$profile_id" -mindepth 1 -delete >/dev/null
    tar -C "$app_root" -cf - . | \
        "${adb[@]}" shell run-as "$package" tar -C "files/apps/$profile_id" -xf -
    ADB="$adb_bin" "$repo_dir/tools/bxapt" --serial "$serial" \
        normalize "$profile_id"
fi
if [[ -n "$runtime_root" ]]; then
    rootfs_id_file="$runtime_root/.bionicx-rootfs-seed-id"
    local_rootfs_id=""
    remote_rootfs_id=""
    if [[ -f "$rootfs_id_file" ]]; then
        local_rootfs_id="$(tr -d '\r\n' < "$rootfs_id_file")"
        remote_rootfs_id="$("${adb[@]}" shell run-as "$package" \
            cat files/rootfs/.bionicx-rootfs-seed-id 2>/dev/null | tr -d '\r\n' || true)"
    fi
    if [[ -n "$local_rootfs_id" && "$local_rootfs_id" == "$remote_rootfs_id" ]]; then
        echo "reusing shared rootfs $local_rootfs_id"
    else
        # A rootfs is an immutable package image.  Remove the previous image
        # before extraction so libraries deleted by a package transition
        # cannot survive a seed transition and mask an incomplete package set.
        "${adb[@]}" shell run-as "$package" find \
            files/rootfs -mindepth 1 -delete >/dev/null
        tar -C "$runtime_root" -cf - . | \
            "${adb[@]}" shell run-as "$package" tar -C files/rootfs -xf -
    fi
fi

temporary="/data/local/tmp/bionicx-profile-$$.json"
"${adb[@]}" push "$profile" "$temporary" >/dev/null
"${adb[@]}" shell run-as "$package" cp "$temporary" files/profiles/active.json
"${adb[@]}" shell rm "$temporary"
echo "installed profile $profile_id for $package"
