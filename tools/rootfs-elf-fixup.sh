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
    dependency_directory() {
        soname=$1
        owner_directory=${file%/*}
        old_ifs=$IFS
        IFS=:
        set -f
        for entry in $rewritten_rpath; do
            directory=$(printf '%s\n' "$entry" | sed \
                -e "s|\${ORIGIN}|$owner_directory|g" \
                -e "s|\$ORIGIN|$owner_directory|g")
            if [ -e "$directory/$soname" ]; then
                set +f
                IFS=$old_ifs
                return 1
            fi
        done
        set +f
        IFS=$old_ifs

        candidates=$(awk -F '\t' -v name="$soname" \
            '$1 == name { print $2 }' "$BIONICX_ELF_INDEX")
        [ -n "$candidates" ] || return 1
        selected=
        for candidate in $candidates; do
            resolved=$(readlink -f "$candidate" 2>/dev/null || printf '%s' "$candidate")
            case " $selected " in
                *" $resolved "*) ;;
                *) selected="$selected $resolved" ;;
            esac
        done
        set -- $selected
        [ "$#" -eq 1 ] || return 1
        directory=${1%/*}
        case "$directory" in
            "$runtime"/*) printf '%s%s\n' "$deploy_root" "${directory#"$runtime"}" ;;
            *) return 1 ;;
        esac
    }
    private_object_directory() {
        directory=$1
        [ -d "$directory" ] || return 1
        for so in "$directory"/*; do
            case "$so" in
                *.so|*.so.*) ;;
                *) continue ;;
            esac
            [ -e "$so" ] || continue
            soname=${so##*/}
            for sysdir in usr/lib usr/lib/aarch64-linux-gnu \
                    lib lib/aarch64-linux-gnu; do
                candidate="$runtime/$sysdir/$soname"
                [ -e "$candidate" ] || continue
                resolved=$(readlink -f "$candidate" 2>/dev/null ||
                    printf '%s' "$candidate")
                ours=$(readlink -f "$so" 2>/dev/null || printf '%s' "$so")
                if [ "$resolved" != "$ours" ]; then
                    return 1
                fi
            done
        done
        return 0
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

        # patchelf --set-rpath moves .gresource.* into a rewritten PT_LOAD
        # and GTK then cannot find /org/gtk/libgtk/ui templates. Leave the
        # original RUNPATH; ld.so.cache covers multiarch directories.
        if run_readelf -S "$file" 2>/dev/null | grep -q ' \.gresource'; then
            printf '%s\tGRESOURCE\tkeep-rpath\n' "$relative" >> "$ledger"
            continue
        fi

        current_rpath=$(run_patchelf --print-rpath "$file" 2>/dev/null || true)
        legacy_rpath=0
        if [ -n "$current_rpath" ] && run_readelf -d "$file" 2>/dev/null | \
                grep -q '(RPATH)'; then
            legacy_rpath=1
        fi
        owner_directory=${file%/*}
        case "$owner_directory" in
            "$runtime"/*)
                object_directory="$deploy_root${owner_directory#"$runtime"}"
                ;;
            *)
                object_directory=$owner_directory
                ;;
        esac
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
            # A previous pass may have appended this directory. Drop it here
            # and place it again after unique-provider resolution.
            [ "$entry" = "$object_directory" ] && continue
            case ":$rewritten_rpath:" in
                *":$entry:"*) ;;
                *) rewritten_rpath="$rewritten_rpath:$entry" ;;
            esac
        done
        set +f
        IFS=$old_ifs
        for soname in $needed; do
            direct_directory=$(dependency_directory "$soname" || true)
            [ -n "$direct_directory" ] || continue
            case ":$rewritten_rpath:" in
                *":$direct_directory:"*) ;;
                *)
                    rewritten_rpath="$rewritten_rpath:$direct_directory"
                    printf '%s\tDT_NEEDED\t%s\t%s\n' "$relative" \
                        "$soname" "$direct_directory" >> "$ledger"
                    ;;
            esac
        done
        # $ORIGIN expands to the path used to open the object. A multiarch
        # symlink (LibreOffice libuno_cppuhelpergcc3.so.3) therefore searches
        # usr/lib/aarch64-linux-gnu, not the private program/ tree that holds
        # siblings such as libreglo.so. Record the real file's directory.
        # Prepend that directory when every colliding system SONAME is only a
        # symlink back here, so dladdr/getUnoIniUri see program/ rather than
        # the symlink path. Append when the directory also ships a real
        # system SONAME (WPS bundled FreeType) so Debian still wins.
        case ":$rewritten_rpath:" in
            *":$object_directory:"*) ;;
            *)
                if private_object_directory "$owner_directory"; then
                    rewritten_rpath="$object_directory:$rewritten_rpath"
                    printf '%s\tOBJECT_DIR\t%s\tprepend\n' "$relative" \
                        "$object_directory" >> "$ledger"
                else
                    rewritten_rpath="$rewritten_rpath:$object_directory"
                    printf '%s\tOBJECT_DIR\t%s\n' "$relative" \
                        "$object_directory" >> "$ledger"
                fi
                ;;
        esac
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

paths_file=
if [ "${1:-}" = --paths-from ]; then
    [ "$#" -ge 3 ] && [ "$#" -le 4 ] || {
        echo "usage: rootfs-elf-fixup.sh --paths-from FILE TREE [RUNTIME_ROOT]" >&2
        exit 2
    }
    paths_file=$2
    shift 2
fi
[ "$#" -ge 1 ] && [ "$#" -le 2 ] || {
    echo "usage: rootfs-elf-fixup.sh [--paths-from FILE] TREE [RUNTIME_ROOT]" >&2
    exit 2
}
tree=$1
runtime=${2:-$tree}
ledger_dir="${BIONICX_LEDGER_DIR:-$tree/var/lib/bionicx}"
ledger="$ledger_dir/elf-fixups.tsv.tmp"
dependency_index="$ledger_dir/elf-files.tsv.tmp"
targets="$ledger_dir/elf-targets.tsv.tmp"
changed_paths="$ledger_dir/elf-changed-paths.tsv.tmp"
mkdir -p "$ledger_dir"
find "$runtime" \( -type f -o -type l \) \
    \( -name '*.so' -o -name '*.so.*' \) -print | while IFS= read -r file; do
    printf '%s\t%s\n' "${file##*/}" "$file"
done | sort -u > "$dependency_index"
export BIONICX_ELF_INDEX="$dependency_index"

if [ -n "$paths_file" ]; then
    [ -f "$paths_file" ] || {
        echo "missing ELF path manifest: $paths_file" >&2
        exit 1
    }
    : > "$targets"
    : > "$changed_paths"
    while IFS= read -r path || [ -n "$path" ]; do
        case "$path" in
            ./*) path=${path#./} ;;
            /*) path=${path#/} ;;
        esac
        case "$path" in
            ''|..|../*|*/..|*/../*) continue ;;
        esac
        printf '/%s\n' "$path" >> "$changed_paths"
        file="$tree/$path"
        [ -f "$file" ] && [ ! -L "$file" ] || continue
        case "${file##*/}" in
            *.so|*.so.*) ;;
            *) [ -x "$file" ] || continue ;;
        esac
        printf '/%s\n' "$path" >> "$targets"
    done < "$paths_file"
    sort -u "$targets" -o "$targets"
    sort -u "$changed_paths" -o "$changed_paths"

    if [ -s "$changed_paths" ] && [ -f "$ledger_dir/elf-fixups.tsv" ]; then
        awk -F '\t' 'NR == FNR { target[$1] = 1; next }
            !($1 in target)' "$changed_paths" \
            "$ledger_dir/elf-fixups.tsv" > "$ledger"
    elif [ -f "$ledger_dir/elf-fixups.tsv" ]; then
        cp "$ledger_dir/elf-fixups.tsv" "$ledger"
    else
        : > "$ledger"
    fi

    set --
    count=0
    while IFS= read -r relative || [ -n "$relative" ]; do
        set -- "$@" "$tree$relative"
        count=$((count + 1))
        if [ "$count" -eq 128 ]; then
            /bin/sh "$0" --files "$tree" "$runtime" "$ledger" "$@"
            set --
            count=0
        fi
    done < "$targets"
    if [ "$#" -gt 0 ]; then
        /bin/sh "$0" --files "$tree" "$runtime" "$ledger" "$@"
    fi
    pruned="$ledger_dir/elf-fixups-pruned.tsv.tmp"
    : > "$pruned"
    tab=$(printf '\t')
    while IFS= read -r line || [ -n "$line" ]; do
        relative=${line%%"$tab"*}
        [ -e "$tree$relative" ] || continue
        printf '%s\n' "$line" >> "$pruned"
    done < "$ledger"
    mv "$pruned" "$ledger"
else
    : > "$ledger"
    for directory in usr opt bin sbin lib lib64; do
        [ -d "$tree/$directory" ] || continue
        find "$tree/$directory" -type f \( -perm /111 -o -name '*.so' \
            -o -name '*.so.*' \) -exec /bin/sh "$0" --files \
            "$tree" "$runtime" "$ledger" {} +
    done
fi
sort -u "$ledger" -o "$ledger"
mv "$ledger" "$ledger_dir/elf-fixups.tsv"
rm -f "$dependency_index" "$targets" "$changed_paths" \
    "$ledger_dir/elf-fixups-pruned.tsv.tmp"
printf 'bionicx ELF fixups: %s entries\n' \
    "$(wc -l < "$ledger_dir/elf-fixups.tsv")"
