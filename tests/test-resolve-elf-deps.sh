#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
temporary_root="${TMPDIR:-/tmp}"
mkdir -p "$temporary_root"
work_dir="$(mktemp -d "$temporary_root/bionicx-resolver-test.XXXXXX")"
trap 'find "$work_dir" -depth -delete' EXIT

cat > "$work_dir/library.c" <<'EOF'
int resolver_test_value(void) { return 42; }
EOF
cat > "$work_dir/client.c" <<'EOF'
extern int resolver_test_value(void);
int main(void) { return resolver_test_value() == 42 ? 0 : 1; }
EOF

cc -fPIC -shared -Wl,-soname,libresolver-alias.so.1 \
    -o "$work_dir/libresolver-actual.so.1.2" "$work_dir/library.c"
ln -s libresolver-actual.so.1.2 "$work_dir/libresolver-alias.so.1"
cc -fPIC -shared -Wl,-soname,libdynamic-alias.so.9 \
    -o "$work_dir/libdynamic-actual.so.9.4" "$work_dir/library.c"
ln -s libdynamic-actual.so.9.4 "$work_dir/libdynamic-alias.so.9"
cc -o "$work_dir/client" "$work_dir/client.c" \
    -L"$work_dir" -Wl,-rpath-link,"$work_dir" -l:libresolver-alias.so.1

mkdir "$work_dir/closure" "$work_dir/application"
mv "$work_dir/client" "$work_dir/application/client"
"$repo_dir/tools/resolve-elf-deps.py" \
    --entry "$work_dir/application/client" \
    --entry "$work_dir/libdynamic-alias.so.9" \
    --search-root "$work_dir" \
    --search-root /lib \
    --search-root /usr/lib \
    --exclude-copy-root "$work_dir/application" \
    --copy-to "$work_dir/closure" \
    --json "$work_dir/closure.json"

test ! -e "$work_dir/closure/client"
test -f "$work_dir/closure/libresolver-alias.so.1"
test ! -e "$work_dir/closure/libresolver-actual.so.1.2"
test -f "$work_dir/closure/libdynamic-alias.so.9"
test ! -e "$work_dir/closure/libdynamic-actual.so.9.4"
LD_LIBRARY_PATH="$work_dir/closure" "$work_dir/application/client"
python3 - "$work_dir/closure.json" <<'PY'
import json
import sys

report = json.load(open(sys.argv[1], encoding="utf-8"))
assert any(
    names == ["libresolver-alias.so.1"]
    for names in report["copyNames"].values()
)
assert any(
    names == ["libdynamic-alias.so.9"]
    for names in report["copyNames"].values()
)
assert report["excludedCopyRoots"] == [sys.argv[1].rsplit("/", 1)[0] + "/application"]
PY

echo "resolve-elf-deps dependency/entry aliases and copy exclusion test: PASS"
