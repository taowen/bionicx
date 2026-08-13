#!/bin/sh
set -eu

root=${BIONICX_ROOTFS:?missing BIONICX_ROOTFS}
archives="$root/var/cache/apt/archives"
manifest=${BIONICX_UNPACK_MANIFEST:?missing BIONICX_UNPACK_MANIFEST}
snapshot=${BIONICX_PACKAGE_SNAPSHOT:?missing BIONICX_PACKAGE_SNAPSHOT}
apt_config=${BIONICX_APT_CONFIG:?missing BIONICX_APT_CONFIG}
requested_state=${BIONICX_REQUESTED_PACKAGES:?missing BIONICX_REQUESTED_PACKAGES}
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
    --unpack-wave)
        remaining=${BIONICX_REMAINING_DEBS:?missing BIONICX_REMAINING_DEBS}
        # Missing list: start from archives. Existing empty list: waves are done.
        if [ ! -f "$remaining" ]; then
            : > "$remaining"
            for archive in "$archives"/*.deb; do
                [ -e "$archive" ] || continue
                printf '%s\n' "$archive" >> "$remaining"
            done
        fi
        [ -s "$remaining" ] || {
            : > "$manifest"
            printf '0\n' > "$remaining.count"
            exit 0
        }
        set --
        while IFS= read -r archive || [ -n "$archive" ]; do
            [ -n "$archive" ] && [ -e "$archive" ] || continue
            set -- "$@" "$archive"
        done < "$remaining"
        [ "$#" -gt 0 ] || { : > "$manifest"; : > "$remaining"; exit 0; }
        "$root/usr/bin/dpkg" --force-not-root --force-script-chrootless \
            --root="$root" --admindir="$root/var/lib/dpkg" --unpack "$@" || true
        : > "$manifest"
        next="$remaining.next"
        : > "$next"
        for archive do
            package="$("$root/usr/bin/dpkg-deb" -f "$archive" Package)"
            [ -n "$package" ] || { printf '%s\n' "$archive" >> "$next"; continue; }
            abbrev="$("$root/usr/bin/dpkg-query" --admindir="$root/var/lib/dpkg" \
                -W '-f=${db:Status-Abbrev}' "$package" 2>/dev/null || true)"
            case "$abbrev" in
                iU*|iF*|iW*|it*|ii*|iH*)
                    "$root/usr/bin/dpkg-deb" --fsys-tarfile "$archive" | \
                        "$root/bin/tar" -tf - >> "$manifest"
                    ;;
                *)
                    printf '%s\n' "$archive" >> "$next"
                    ;;
            esac
        done
        mv "$next" "$remaining"
        count=$(wc -l < "$remaining")
        count=$(printf '%s' "$count" | tr -d ' \t')
        printf '%s\n' "$count" > "$remaining.count"
        printf 'bionicx unpack wave remaining=%s\n' "$count"
        ;;
    --snapshot)
        "$root/usr/bin/dpkg-query" --admindir="$root/var/lib/dpkg" \
            -W '-f=${Package} ${db:Status-Abbrev}\n' 2>/dev/null | \
            awk '$2 == "ii" { print $1 }' | sort -u > "$snapshot"
        ;;
    --record-requested)
        shift
        : > "$requested_state"
        for package do
            printf '%s\n' "$package" >> "$requested_state"
        done
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
        echo "usage: bxapt-unpack.sh --clear|--unpack|--unpack-wave|--snapshot|--record-requested|--reconcile" >&2
        exit 2
        ;;
esac
