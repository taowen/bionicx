#!/usr/bin/env bash
# Host mock: static-PIE ldconfig cannot chroot or write Android /etc.
# The wrapper must keep the cache inside BIONICX_ROOTFS and drop -r.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$repo_dir/build/test-bxapt-ldconfig"
root="$test_dir/root"
rm -rf "$test_dir"
mkdir -p "$root/sbin" "$root/usr/sbin" "$root/etc"

# Fake a static-PIE ldconfig (ELF magic only; --install must save it).
printf '\177ELF' > "$root/sbin/ldconfig"
chmod 0755 "$root/sbin/ldconfig"
cat > "$root/etc/ld.so.conf" <<'EOF'
include /etc/ld.so.conf.d/*.conf
EOF

BIONICX_ROOTFS="$root" /bin/sh "$repo_dir/tools/bxapt-ldconfig.sh" --install \
    "$repo_dir/tools/bxapt-ldconfig.sh"
grep -q 'bionicx ldconfig wrapper' "$root/sbin/ldconfig"
test -s "$root/sbin/ldconfig.bionicx-real"
# Re-install must be idempotent and keep the saved ELF.
first_real=$(wc -c < "$root/sbin/ldconfig.bionicx-real")
BIONICX_ROOTFS="$root" /bin/sh "$root/sbin/ldconfig" --install \
    "$repo_dir/tools/bxapt-ldconfig.sh"
[[ "$(wc -c < "$root/sbin/ldconfig.bionicx-real")" == "$first_real" ]]

cat > "$root/sbin/ldconfig.bionicx-real" <<'EOF'
#!/bin/sh
printf 'ldconfig'
for argument do
    printf ' %s' "$argument"
done
printf '\n'
EOF
chmod 0755 "$root/sbin/ldconfig.bionicx-real"

out=$(BIONICX_ROOTFS="$root" /bin/sh "$root/sbin/ldconfig" \
    -r "$root/" --verbose)
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -F -- "-C $root/etc/ld.so.cache" >/dev/null
printf '%s\n' "$out" | grep -F -- "-f $root/etc/ld.so.conf" >/dev/null
printf '%s\n' "$out" | grep -F -- --verbose >/dev/null
if printf '%s\n' "$out" | grep -Eq -- ' -r | --root '; then
    echo "wrapper leaked a chroot argument: $out" >&2
    exit 1
fi

out=$(BIONICX_ROOTFS="$root" /bin/sh "$root/sbin/ldconfig")
printf '%s\n' "$out" | grep -F -- "-C $root/etc/ld.so.cache" >/dev/null

echo "bxapt ldconfig wrapper keeps the cache in the rootfs: PASS"
