#!/bin/sh
# Debian libc-bin ships a static-PIE ldconfig. It cannot use LD_PRELOAD, so
# `ldconfig -r "$DPKG_ROOT"` hits a real chroot(2) and a bare `ldconfig`
# writes Android's read-only /etc/ld.so.cache. Rewrite both into a cache
# file under BIONICX_ROOTFS. Maintainer scripts still exec /sbin/ldconfig.
set -eu

root=${BIONICX_ROOTFS:?missing BIONICX_ROOTFS}

install_wrapper() {
    target=$root/sbin/ldconfig
    if [ -L "$target" ]; then
        target=$(readlink -f "$target")
    fi
    [ -e "$target" ] || target=$root/usr/sbin/ldconfig
    [ -e "$target" ] || {
        echo "bxapt-ldconfig: missing $root/sbin/ldconfig" >&2
        exit 1
    }
    case "$(od -An -N4 -tx1 "$target" 2>/dev/null)" in
        *7f*45*4c*46*)
            cp "$target" "$root/sbin/ldconfig.bionicx-real"
            ;;
        *)
            if grep -q 'bionicx ldconfig wrapper' "$target" 2>/dev/null; then
                if [ "$1" != "$target" ]; then
                    cp "$1" "$target"
                    chmod 0755 "$target"
                fi
                return 0
            fi
            echo "bxapt-ldconfig: refusing to replace $target" >&2
            exit 1
            ;;
    esac
    cp "$1" "$target"
    chmod 0755 "$target"
    if [ -e "$root/usr/sbin/ldconfig" ] && [ "$root/usr/sbin/ldconfig" != "$target" ]; then
        if [ -L "$root/usr/sbin/ldconfig" ]; then
            :
        else
            cp "$1" "$root/usr/sbin/ldconfig"
            chmod 0755 "$root/usr/sbin/ldconfig"
        fi
    fi
}

if [ "${1:-}" = --install ]; then
    shift
    self=$0
    [ "$#" -eq 0 ] || self=$1
    install_wrapper "$self"
    exit 0
fi

real=$root/sbin/ldconfig.bionicx-real
if [ ! -x "$real" ]; then
    echo "bxapt-ldconfig: missing $real (run --install)" >&2
    exit 1
fi

skip=
filtered=
for argument do
    if [ -n "$skip" ]; then
        skip=
        continue
    fi
    case "$argument" in
        -r|--root|-C|--cache-file|-f|--config)
            skip=1
            ;;
        -r*|-C*)
            ;;
        *)
            filtered="$filtered $argument"
            ;;
    esac
done

# Word-splitting $filtered is intentional: these are rewritten ldconfig flags.
# shellcheck disable=SC2086
exec "$real" \
    -C "$root/etc/ld.so.cache" \
    -f "$root/etc/ld.so.conf" \
    $filtered
