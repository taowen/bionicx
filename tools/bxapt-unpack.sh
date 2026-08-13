#!/bin/sh
set -eu

root=${BIONICX_ROOTFS:?missing BIONICX_ROOTFS}
archives="$root/var/cache/apt/archives"
manifest=${BIONICX_UNPACK_MANIFEST:?missing BIONICX_UNPACK_MANIFEST}
snapshot=${BIONICX_PACKAGE_SNAPSHOT:?missing BIONICX_PACKAGE_SNAPSHOT}
apt_config=${BIONICX_APT_CONFIG:?missing BIONICX_APT_CONFIG}
case "${1:-}" in
    --clear)
        find "$archives" -maxdepth 1 -type f -name '*.deb' -delete
        : > "$manifest"
        ;;
    --unpack)
        shift
        if [ "$#" -eq 0 ]; then
            set -- "$archives"/*.deb
        fi
        [ -e "$1" ] || exit 0
        : > "$manifest"
        for archive do
            "$root/usr/bin/dpkg-deb" --fsys-tarfile "$archive" | \
                "$root/bin/tar" -tf - >> "$manifest"
        done
        "$root/usr/bin/dpkg" --force-not-root --force-script-chrootless \
            --root="$root" --admindir="$root/var/lib/dpkg" --unpack "$@"
        ;;
    --snapshot)
        "$root/usr/bin/dpkg-query" --admindir="$root/var/lib/dpkg" \
            -W '-f=${Package} ${db:Status-Abbrev}\n' 2>/dev/null | \
            awk '$2 == "ii" { print $1 }' | sort -u > "$snapshot"
        ;;
    --reconcile)
        shift
        current="$snapshot.current"
        requested="$snapshot.requested"
        : > "$requested"
        for package do
            printf '%s\n' "$package" >> "$requested"
        done
        "$root/usr/bin/dpkg-query" --admindir="$root/var/lib/dpkg" \
            -W '-f=${Package} ${db:Status-Abbrev}\n' 2>/dev/null | \
            awk '$2 == "ii" { print $1 }' | sort -u > "$current"
        set --
        while IFS= read -r package || [ -n "$package" ]; do
            [ -n "$package" ] || continue
            set -- "$@" "$package"
        done <<EOF
$(comm -13 "$snapshot" "$current")
EOF
        if [ "$#" -gt 0 ]; then
            "$root/usr/bin/apt-mark" -c "$apt_config" auto "$@"
        fi
        set --
        while IFS= read -r package || [ -n "$package" ]; do
            [ -n "$package" ] || continue
            grep -Fx "$package" "$current" >/dev/null || continue
            set -- "$@" "$package"
        done < "$requested"
        if [ "$#" -gt 0 ]; then
            "$root/usr/bin/apt-mark" -c "$apt_config" manual "$@"
        fi
        rm -f "$current" "$requested"
        ;;
    *)
        echo "usage: bxapt-unpack.sh --clear|--unpack|--snapshot|--reconcile" >&2
        exit 2
        ;;
esac
