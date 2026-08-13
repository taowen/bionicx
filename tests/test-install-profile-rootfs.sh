#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$repo_dir/build/test-install-profile-rootfs"
rm -rf "$test_dir"
mkdir -p "$test_dir/bundle/rootfs/usr" "$test_dir/empty"

if "$repo_dir/tools/install-profile.sh" \
        --profile "$repo_dir/profiles/hello.json" \
        --runtime-root "$test_dir/bundle" 2>"$test_dir/bundle.err"; then
    echo "seed bundle directory was accepted as a rootfs" >&2
    exit 1
fi
grep -F "use $test_dir/bundle/rootfs" "$test_dir/bundle.err" >/dev/null

if "$repo_dir/tools/install-profile.sh" \
        --profile "$repo_dir/profiles/hello.json" \
        --runtime-root "$test_dir/empty" 2>"$test_dir/empty.err"; then
    echo "directory without usr/ was accepted as a rootfs" >&2
    exit 1
fi
grep -F 'missing usr/' "$test_dir/empty.err" >/dev/null

if "$repo_dir/tools/install-profile.sh" --help 2>"$test_dir/help.err"; then
    :
fi
grep -F -- '--replace-rootfs' "$test_dir/help.err" >/dev/null

# ELF normalize uses the seed's /bin/sh and patchelf. The shipped installer
# must extract the rootfs before it calls bxapt normalize.
awk '
    /^[[:space:]]*#/ { next }
    /tar -C files\/rootfs/ { rootfs=NR }
    /normalize "\$profile_id"/ { normalize=NR }
    END {
        if (rootfs == 0 || normalize == 0 || rootfs >= normalize) {
            print "rootfs extract must precede bxapt normalize" > "/dev/stderr"
            exit 1
        }
    }
' "$repo_dir/tools/install-profile.sh"

mkdir -p "$test_dir/nogl"
if "$repo_dir/tools/install-profile.sh" \
        --profile "$repo_dir/profiles/krita.json" \
        --app-root "$test_dir/nogl" 2>"$test_dir/nogl.err"; then
    echo "krita without Gladio libGL was accepted" >&2
    exit 1
fi
grep -F 'needs Gladio libGL' "$test_dir/nogl.err" >/dev/null

echo "install-profile rootfs path check: PASS"
