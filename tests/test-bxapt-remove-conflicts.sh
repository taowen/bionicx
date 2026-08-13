#!/usr/bin/env bash
# Host mock: unpacking systemd.deb must remove seed systemd-standalone-sysusers.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$repo_dir/build/test-bxapt-remove-conflicts"
root="$test_dir/root"
run_dir="$test_dir/run"
archives="$root/var/cache/apt/archives"
rm -rf "$test_dir"
mkdir -p "$archives" "$root/var/lib/dpkg" "$root/bin" "$root/usr/bin" "$run_dir"

state="$run_dir/dpkg-state"
printf 'systemd-standalone-sysusers ii\n' > "$state"
: > "$run_dir/dpkg.log"

cat > "$root/usr/bin/dpkg" <<'EOF'
#!/bin/sh
printf 'dpkg %s\n' "$*" >> "$BIONICX_TEST_DPKG_LOG"
mode=
for argument do
    case "$argument" in
        --remove) mode=remove ;;
    esac
done
[ "$mode" = remove ] || exit 0
package=
for argument do
    case "$argument" in
        --*) ;;
        *) package=$argument ;;
    esac
done
tmp="$BIONICX_TEST_DPKG_STATE.next"
awk -v package="$package" '$1 != package { print }' \
    "$BIONICX_TEST_DPKG_STATE" > "$tmp"
mv "$tmp" "$BIONICX_TEST_DPKG_STATE"
EOF
cat > "$root/usr/bin/dpkg-query" <<'EOF'
#!/bin/sh
package=
for argument do
    case "$argument" in
        --admindir=*|-W|-f=*) ;;
        *) package=$argument ;;
    esac
done
awk -v package="$package" '$1 == package { print $2; exit }' \
    "$BIONICX_TEST_DPKG_STATE"
EOF
chmod +x "$root/usr/bin/dpkg" "$root/usr/bin/dpkg-query"

export BIONICX_ROOTFS="$root"
export BIONICX_UNPACK_MANIFEST="$run_dir/unpacked-paths.txt"
export BIONICX_PACKAGE_SNAPSHOT="$run_dir/installed-packages-before.txt"
export BIONICX_REQUESTED_PACKAGES="$run_dir/requested-packages.txt"
export BIONICX_APT_CONFIG="$run_dir/apt.conf"
export BIONICX_TEST_DPKG_STATE="$state"
export BIONICX_TEST_DPKG_LOG="$run_dir/dpkg.log"
: > "$BIONICX_UNPACK_MANIFEST"
: > "$BIONICX_PACKAGE_SNAPSHOT"
: > "$BIONICX_REQUESTED_PACKAGES"

# No systemd archive and no replacement flag: leave standalone installed.
/bin/sh "$repo_dir/tools/bxapt-unpack.sh" --remove-conflicts
grep -Fx 'systemd-standalone-sysusers ii' "$state" >/dev/null
test ! -s "$run_dir/dpkg.log"

# Requesting systemd must remove standalone before apt can download.
BIONICX_REPLACING_SYSTEMD=1 \
    /bin/sh "$repo_dir/tools/bxapt-unpack.sh" --remove-conflicts
grep -F 'systemd-standalone-sysusers ii' "$state" >/dev/null && {
    echo "standalone sysusers survived BIONICX_REPLACING_SYSTEMD" >&2
    exit 1
}
grep -F -- '--remove --force-depends systemd-standalone-sysusers' \
    "$run_dir/dpkg.log" >/dev/null

printf 'systemd-standalone-sysusers ii\n' > "$state"
: > "$run_dir/dpkg.log"
unset BIONICX_REPLACING_SYSTEMD
touch "$archives/systemd_257.13-1_arm64.deb"
/bin/sh "$repo_dir/tools/bxapt-unpack.sh" --remove-conflicts
grep -F 'systemd-standalone-sysusers ii' "$state" >/dev/null && {
    echo "standalone sysusers survived systemd.deb" >&2
    exit 1
}
grep -F -- '--remove --force-depends systemd-standalone-sysusers' \
    "$run_dir/dpkg.log" >/dev/null

echo "bxapt removes standalone sysusers before systemd: PASS"
