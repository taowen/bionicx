#!/usr/bin/env bash
# Host mock: dpkg will not unpack a package whose Pre-Depends parent is only
# unpacked. bxapt must configure each wave before the next unpack.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$repo_dir/build/test-bxapt-unpack-waves"
root="$test_dir/root"
run_dir="$test_dir/run"
archives="$root/var/cache/apt/archives"
rm -rf "$test_dir"
mkdir -p "$archives" "$root/var/lib/dpkg" "$root/bin" "$root/usr/bin" "$run_dir"

state="$run_dir/dpkg-state"
predepends="$run_dir/predepends"
: > "$state"
printf '%s\n' \
    'libreoffice-common ucf' \
    'libreoffice-writer libreoffice-common' \
    'python3-minimal python3.13-minimal' > "$predepends"

cat > "$root/usr/bin/dpkg" <<'EOF'
#!/bin/sh
set -eu
state=$BIONICX_TEST_DPKG_STATE
predepends=$BIONICX_TEST_PREDEPENDS
mode=
for argument do
    case "$argument" in
        --unpack) mode=unpack ;;
        --configure) mode=configure ;;
    esac
done
status_of() {
    awk -v package="$1" '$1 == package { print $2; exit }' "$state"
}
set_status() {
    local tmp="$state.next"
    awk -v package="$1" '$1 != package { print }' "$state" > "$tmp"
    printf '%s %s\n' "$1" "$2" >> "$tmp"
    mv "$tmp" "$state"
}
if [ "$mode" = configure ]; then
    tmp="$state.next"
    : > "$tmp"
    while IFS= read -r line || [ -n "$line" ]; do
        [ -n "$line" ] || continue
        set -- $line
        if [ "${2:-}" = iU ]; then
            printf '%s ii\n' "$1" >> "$tmp"
        else
            printf '%s\n' "$line" >> "$tmp"
        fi
    done < "$state"
    mv "$tmp" "$state"
    exit 0
fi
if [ "$mode" != unpack ]; then
    printf 'dpkg %s\n' "$*" >&2
    exit 2
fi
failed=0
for archive do
    case "$archive" in
        --*) continue ;;
        *.deb) ;;
        *) continue ;;
    esac
    package=$(basename "$archive" .deb)
    parent=$(awk -v package="$package" '$1 == package { print $2; exit }' \
        "$predepends")
    if [ -n "$parent" ] && [ "$(status_of "$parent")" != ii ]; then
        printf 'dpkg: regarding %s containing %s, pre-dependency problem:\n' \
            "$archive" "$package" >&2
        printf ' %s pre-depends on %s\n' "$package" "$parent" >&2
        failed=1
        continue
    fi
    set_status "$package" iU
done
exit "$failed"
EOF

cat > "$root/usr/bin/dpkg-query" <<'EOF'
#!/bin/sh
set -eu
state=$BIONICX_TEST_DPKG_STATE
package=
format=
for argument do
    case "$argument" in
        --admindir=*) ;;
        -W) ;;
        -f=*) format=${argument#-f=} ;;
        -f) ;;
        *)
            if [ -z "$package" ] && [ "${argument#-}" = "$argument" ]; then
                package=$argument
            else
                format=$argument
            fi
            ;;
    esac
done
if [ -n "$package" ]; then
    abbrev=$(awk -v package="$package" '$1 == package { print $2; exit }' "$state")
    [ -n "$abbrev" ] || exit 1
    printf '%s\n' "$abbrev"
    exit 0
fi
awk '{ printf "%s %s\n", $1, $2 }' "$state"
EOF

cat > "$root/usr/bin/dpkg-deb" <<'EOF'
#!/bin/sh
set -eu
case "$1" in
    -f)
        basename "$2" .deb
        ;;
    --fsys-tarfile)
        printf './usr/share/doc/%s\n' "$(basename "$2" .deb)"
        ;;
    *)
        echo "dpkg-deb $*" >&2
        exit 2
        ;;
esac
EOF

cat > "$root/bin/tar" <<'EOF'
#!/bin/sh
cat
EOF
chmod +x "$root/usr/bin/dpkg" "$root/usr/bin/dpkg-query" \
    "$root/usr/bin/dpkg-deb" "$root/bin/tar"

touch "$archives/ucf.deb" \
    "$archives/python3.13-minimal.deb" \
    "$archives/libreoffice-common.deb" \
    "$archives/libreoffice-writer.deb" \
    "$archives/python3-minimal.deb"

manifest="$run_dir/unpacked-paths.txt"
snapshot="$run_dir/installed-packages-before.txt"
requested="$run_dir/requested-packages.txt"
remaining="$run_dir/remaining-debs.txt"
: > "$manifest"
: > "$snapshot"
: > "$requested"

export BIONICX_ROOTFS="$root"
export BIONICX_UNPACK_MANIFEST="$manifest"
export BIONICX_PACKAGE_SNAPSHOT="$snapshot"
export BIONICX_REQUESTED_PACKAGES="$requested"
export BIONICX_REMAINING_DEBS="$remaining"
export BIONICX_APT_CONFIG="$run_dir/apt.conf"
export BIONICX_TEST_DPKG_STATE="$state"
export BIONICX_TEST_PREDEPENDS="$predepends"

status_of() {
    awk -v package="$1" '$1 == package { print $2; exit }' "$state"
}

run_wave() {
    /bin/sh "$repo_dir/tools/bxapt-unpack.sh" --unpack-wave
}

configure_wave() {
    "$root/usr/bin/dpkg" --configure -a
}

# One-shot unpack cannot install a Pre-Depends child while the parent is only
# unpacked, which is the reconstruct failure mode.
set +e
/bin/sh "$repo_dir/tools/bxapt-unpack.sh" --unpack \
    "$archives/ucf.deb" "$archives/libreoffice-common.deb"
unpack_status=$?
set -e
[[ "$unpack_status" -ne 0 ]]
[[ "$(status_of ucf)" == iU ]]
[[ -z "$(status_of libreoffice-common)" ]]

: > "$state"
rm -f "$remaining" "$remaining.count"

run_wave
[[ "$(status_of ucf)" == iU ]]
[[ "$(status_of python3.13-minimal)" == iU ]]
[[ -z "$(status_of libreoffice-common)" ]]
[[ -z "$(status_of python3-minimal)" ]]
[[ -z "$(status_of libreoffice-writer)" ]]
[[ "$(cat "$remaining.count")" == 3 ]]
grep -F "$archives/libreoffice-common.deb" "$remaining" >/dev/null
grep -F "$archives/libreoffice-writer.deb" "$remaining" >/dev/null
grep -F "$archives/python3-minimal.deb" "$remaining" >/dev/null
grep -F './usr/share/doc/ucf' "$manifest" >/dev/null

configure_wave
[[ "$(status_of ucf)" == ii ]]
[[ "$(status_of python3.13-minimal)" == ii ]]

run_wave
[[ "$(status_of libreoffice-common)" == iU ]]
[[ "$(status_of python3-minimal)" == iU ]]
[[ -z "$(status_of libreoffice-writer)" ]]
[[ "$(cat "$remaining.count")" == 1 ]]
grep -F "$archives/libreoffice-writer.deb" "$remaining" >/dev/null

configure_wave
[[ "$(status_of libreoffice-common)" == ii ]]

run_wave
[[ "$(status_of libreoffice-writer)" == iU ]]
[[ "$(cat "$remaining.count")" == 0 ]]
test ! -s "$remaining"

configure_wave
[[ "$(status_of libreoffice-writer)" == ii ]]
[[ "$(status_of python3-minimal)" == ii ]]

# A second empty wave must stay empty and not reopen finished archives.
run_wave
[[ "$(cat "$remaining.count")" == 0 ]]
[[ "$(status_of libreoffice-writer)" == ii ]]

echo "bxapt unpack waves honor Pre-Depends: PASS"
