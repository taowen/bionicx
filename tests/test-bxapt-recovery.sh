#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$repo_dir/build/test-bxapt-recovery"
root="$test_dir/root"
run_dir="$test_dir/run"
rm -rf "$test_dir"
mkdir -p "$root/var/cache/apt/archives" "$root/var/lib/dpkg" "$root/bin" "$root/usr/bin" "$run_dir"

state="$run_dir/dpkg-state"
printf 'seed ii\n' > "$state"

cat > "$root/usr/bin/dpkg-query" <<'EOF'
#!/bin/sh
cat "$BIONICX_TEST_DPKG_STATE"
EOF
cat > "$root/usr/bin/dpkg" <<'EOF'
#!/bin/sh
printf 'dpkg %s\n' "$*"
EOF
cat > "$root/usr/bin/apt-get" <<'EOF'
#!/bin/sh
printf 'apt-get %s\n' "$*"
EOF
cat > "$root/usr/bin/apt-mark" <<'EOF'
#!/bin/sh
printf 'apt-mark %s\n' "$*" >> "$BIONICX_TEST_APT_MARK_LOG"
EOF
cat > "$root/bin/tar" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$root"/usr/bin/* "$root/bin/tar"

mark_log="$run_dir/apt-mark.log"
export BIONICX_TEST_DPKG_STATE="$state"
export BIONICX_TEST_APT_MARK_LOG="$mark_log"

manifest="$run_dir/unpacked-paths.txt"
snapshot="$run_dir/installed-packages-before.txt"
requested="$run_dir/requested-packages.txt"
printf '%s\n' "$root/usr/bin/example" > "$manifest"
printf 'seed\n' > "$snapshot"
printf 'cups-client\n' > "$requested"

BIONICX_ROOTFS="$root" BIONICX_UNPACK_MANIFEST="$manifest" \
BIONICX_PACKAGE_SNAPSHOT="$snapshot" BIONICX_REQUESTED_PACKAGES="$requested" \
BIONICX_APT_CONFIG="$run_dir/apt.conf" \
    /bin/sh "$repo_dir/tools/bxapt-unpack.sh" --record-requested cups-daemon cups-client
grep -Fx cups-daemon "$requested" >/dev/null
grep -Fx cups-client "$requested" >/dev/null

BIONICX_ROOTFS="$root" BIONICX_UNPACK_MANIFEST="$manifest" \
BIONICX_PACKAGE_SNAPSHOT="$snapshot" BIONICX_REQUESTED_PACKAGES="$requested" \
BIONICX_APT_CONFIG="$run_dir/apt.conf" \
    /bin/sh "$repo_dir/tools/bxapt-unpack.sh" --snapshot
printf 'seed ii\nlibcups2 ii\ncups-client ii\n' > "$state"
printf 'cups-client\n' > "$requested"
BIONICX_ROOTFS="$root" BIONICX_UNPACK_MANIFEST="$manifest" \
BIONICX_PACKAGE_SNAPSHOT="$snapshot" BIONICX_REQUESTED_PACKAGES="$requested" \
BIONICX_APT_CONFIG="$run_dir/apt.conf" \
    /bin/sh "$repo_dir/tools/bxapt-unpack.sh" --reconcile cups-client
grep -F 'apt-mark -c '"$run_dir/apt.conf"' auto cups-client libcups2' "$mark_log" >/dev/null
grep -F 'apt-mark -c '"$run_dir/apt.conf"' manual cups-client' "$mark_log" >/dev/null

touch "$root/var/cache/apt/archives/old.deb"
BIONICX_ROOTFS="$root" BIONICX_UNPACK_MANIFEST="$manifest" \
BIONICX_PACKAGE_SNAPSHOT="$snapshot" BIONICX_REQUESTED_PACKAGES="$requested" \
BIONICX_APT_CONFIG="$run_dir/apt.conf" \
    /bin/sh "$repo_dir/tools/bxapt-unpack.sh" --clear
test ! -e "$root/var/cache/apt/archives/old.deb"
test ! -s "$manifest"
echo "bxapt recovery metadata and marks: PASS"
