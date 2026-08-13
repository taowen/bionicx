#!/bin/sh
set -eu

if [ "${1:-}" = --files ]; then
    shift
    tree=$1
    runtime=$2
    ledger=$3
    shift 3
    loader="${BIONICX_INTERPRETER:-$runtime/usr/lib/ld-linux-aarch64.so.1}"
    deploy_root="${BIONICX_DEPLOY_ROOT:-$runtime}"
    system_runpath="$deploy_root/usr/lib:$deploy_root/usr/lib/aarch64-linux-gnu:$deploy_root/lib:$deploy_root/lib/aarch64-linux-gnu:\$ORIGIN:\$ORIGIN/../lib"
    if [ -n "${BIONICX_ROOT_ALIAS:-}" ]; then
        root_alias=$BIONICX_ROOT_ALIAS
    else
        case "$runtime" in
            /data/user/0/*) root_alias="/data/data/${runtime#/data/user/0/}" ;;
            /data/data/*) root_alias="/data/user/0/${runtime#/data/data/}" ;;
            *) root_alias=$runtime ;;
        esac
    fi
    run_patchelf() {
        if [ -n "${BIONICX_PATCHELF:-}" ]; then
            "$BIONICX_PATCHELF" "$@"
            return
        fi
        "$runtime/usr/bin/patchelf" "$@"
    }
    run_readelf() {
        if [ -n "${BIONICX_READELF:-}" ]; then
            "$BIONICX_READELF" "$@"
            return
        fi
        "$runtime/usr/bin/readelf" "$@"
    }
    update_patchelf() {
        temporary="$1.bionicx-new.$$"
        file=$1
        shift
        run_patchelf "$@" --output "$temporary" "$file"
        mv "$temporary" "$file"
    }
    for file do
        # Executable scripts and static ELFs are deliberately outside the
        # dynamic-loader contract.
        needed=$(run_patchelf --print-needed "$file" 2>/dev/null) || continue
        relative=${file#"$tree"}
        current_interpreter=$(run_patchelf --print-interpreter "$file" \
            2>/dev/null || true)
        # The dynamic loader has neither an interpreter nor DT_NEEDED. Adding
        # a dynamic tag to it changes its bootstrap layout and is forbidden.
        [ -n "$current_interpreter" ] || [ -n "$needed" ] || continue
        if [ -n "$current_interpreter" ] && \
                [ "$current_interpreter" != "$loader" ]; then
            update_patchelf "$file" --set-interpreter "$loader"
            printf '%s\tPT_INTERP\t%s\t%s\n' "$relative" \
                "$current_interpreter" "$loader" >> "$ledger"
        fi

        current_rpath=$(run_patchelf --print-rpath "$file" 2>/dev/null || true)
        legacy_rpath=0
        if [ -n "$current_rpath" ] && run_readelf -d "$file" 2>/dev/null | \
                grep -q '(RPATH)'; then
            legacy_rpath=1
        fi
        rewritten_rpath=$system_runpath
        changed=0
        old_ifs=$IFS
        IFS=:
        set -f
        for entry in $current_rpath; do
            case "$entry" in
                "$runtime$root_alias"/*) entry=${entry#"$runtime"}; changed=1 ;;
                "$root_alias$runtime"/*) entry=${entry#"$root_alias"}; changed=1 ;;
                "$runtime"/*|"$root_alias"/*) ;;
                /*) entry="$deploy_root$entry"; changed=1 ;;
            esac
            case ":$rewritten_rpath:" in
                *":$entry:"*) ;;
                *) rewritten_rpath="$rewritten_rpath:$entry" ;;
            esac
        done
        set +f
        IFS=$old_ifs
        if [ "$current_rpath" != "$rewritten_rpath" ] ||
                [ "$changed" -eq 1 ] || [ "$legacy_rpath" -eq 1 ]; then
            update_patchelf "$file" --set-rpath "$rewritten_rpath"
            current_rpath=$rewritten_rpath
        fi

        if [ "$legacy_rpath" -eq 1 ]; then
            printf '%s\tDT_RPATH\tDT_RUNPATH\n' "$relative" >> "$ledger"
        fi

        case "$current_rpath" in
            *"$runtime"*|*"$root_alias"*)
                printf '%s\tRUNPATH\t%s\n' "$relative" \
                    "$current_rpath" >> "$ledger"
                ;;
        esac
    done
    exit
fi

[ "$#" -ge 1 ] && [ "$#" -le 2 ] || {
    echo "usage: rootfs-elf-fixup.sh TREE [RUNTIME_ROOT]" >&2
    exit 2
}
tree=$1
runtime=${2:-$tree}
ledger_dir="${BIONICX_LEDGER_DIR:-$tree/var/lib/bionicx}"
ledger="$ledger_dir/elf-fixups.tsv.tmp"
mkdir -p "$ledger_dir"
: > "$ledger"

for directory in usr opt bin sbin lib lib64; do
    [ -d "$tree/$directory" ] || continue
    find "$tree/$directory" -type f \( -perm /111 -o -name '*.so' \
        -o -name '*.so.*' \) -exec /bin/sh "$0" --files \
        "$tree" "$runtime" "$ledger" {} +
done
sort -u "$ledger" -o "$ledger"
mv "$ledger" "$ledger_dir/elf-fixups.tsv"
printf 'bionicx ELF fixups: %s entries\n' \
    "$(wc -l < "$ledger_dir/elf-fixups.tsv")"
