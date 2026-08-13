#!/bin/sh
set -eu

root=${BIONICX_ROOTFS:?missing BIONICX_ROOTFS}
archives="$root/var/cache/apt/archives"
case "${1:-}" in
    --clear)
        find "$archives" -maxdepth 1 -type f -name '*.deb' -delete
        ;;
    --unpack)
        set -- "$archives"/*.deb
        [ -e "$1" ] || exit 0
        exec "$root/usr/bin/dpkg" --force-not-root --force-script-chrootless \
            --root="$root" --admindir="$root/var/lib/dpkg" --unpack "$@"
        ;;
    *)
        echo "usage: bxapt-unpack.sh --clear|--unpack" >&2
        exit 2
        ;;
esac
