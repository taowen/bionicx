#!/bin/sh
set -eu

if [ "${1:-}" = --files ]; then
    shift
    root=$1
    loader=$2
    ledger=$3
    shift 3
    if [ -n "${BIONICX_ROOT_ALIAS:-}" ]; then
        root_alias=$BIONICX_ROOT_ALIAS
    else
        case "$root" in
            /data/user/0/*) root_alias="/data/data/${root#/data/user/0/}" ;;
            /data/data/*) root_alias="/data/user/0/${root#/data/data/}" ;;
            *) root_alias=$root ;;
        esac
    fi
    run_patchelf() {
        if [ -n "${BIONICX_PATCHELF:-}" ]; then
            "$BIONICX_PATCHELF" "$@"
            return
        fi
        "$loader" --library-path \
            "$root/usr/lib:$root/usr/lib/aarch64-linux-gnu" \
            "$root/usr/bin/patchelf" "$@"
    }
    for file do
        current_rpath=$(run_patchelf --print-rpath "$file" 2>/dev/null || true)
        rewritten_rpath=
        changed=0
        old_ifs=$IFS
        IFS=:
        set -f
        for entry in $current_rpath; do
            case "$entry" in
                "$root$root_alias"/*) entry=${entry#"$root"}; changed=1 ;;
                "$root_alias$root"/*) entry=${entry#"$root_alias"}; changed=1 ;;
                "$root"/*|"$root_alias"/*) ;;
                /*) entry="$root$entry"; changed=1 ;;
            esac
            if [ -n "$rewritten_rpath" ]; then
                rewritten_rpath="$rewritten_rpath:$entry"
            else
                rewritten_rpath=$entry
            fi
        done
        set +f
        IFS=$old_ifs
        if [ "$changed" -eq 1 ]; then
            run_patchelf --set-rpath "$rewritten_rpath" "$file"
            current_rpath=$rewritten_rpath
        fi

        case "$current_rpath" in
            *"$root"*|*"$root_alias"*)
                relative=${file#"$root"}
                printf '%s\t%s\n' "$relative" "$current_rpath" >> "$ledger"
                ;;
        esac
    done
    exit
fi

[ "$#" -eq 2 ] || {
    echo "usage: rootfs-elf-fixup.sh ROOT INTERPRETER" >&2
    exit 2
}
root=$1
loader=$2
ledger_dir="$root/var/lib/bionicx"
ledger="$ledger_dir/elf-fixups.tsv.tmp"
mkdir -p "$ledger_dir"
: > "$ledger"

for directory in usr opt bin sbin lib lib64; do
    [ -d "$root/$directory" ] || continue
    find "$root/$directory" -type f \( -perm /111 -o -name '*.so' \
        -o -name '*.so.*' \) -exec /bin/sh "$0" --files \
        "$root" "$loader" "$ledger" {} +
done
sort -u "$ledger" -o "$ledger"
mv "$ledger" "$ledger_dir/elf-fixups.tsv"
printf 'bionicx ELF fixups: %s entries\n' \
    "$(wc -l < "$ledger_dir/elf-fixups.tsv")"
