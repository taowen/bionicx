#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
grep -F 'gred-ckbi' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'trust-anchors' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'PK11_ListCerts' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'NSS_NoDB_Init' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'gred_nss3' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'tls-example' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'nss-sql-init' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'NSS_Initialize' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'gred-softokn-soname' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'psm-key-slot' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F '0x30' \
    "$repo_dir/examples/nss-ckbi-probe/nss-ckbi-probe.c" >/dev/null
grep -F 'BIONICX_DNS_SERVERS' \
    "$repo_dir/examples/nss-ckbi-probe/install-and-run.sh" >/dev/null
grep -F 'libnssckbi.so' \
    "$repo_dir/examples/nss-ckbi-probe/install-and-run.sh" >/dev/null
grep -F '../aarch64-linux-gnu/libnssckbi.so' \
    "$repo_dir/examples/nss-ckbi-probe/install-and-run.sh" >/dev/null
grep -F 'failed=0' \
    "$repo_dir/examples/nss-ckbi-probe/install-and-run.sh" >/dev/null
grep -F -- '--set-interpreter' \
    "$repo_dir/examples/nss-ckbi-probe/install-and-run.sh" >/dev/null
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/nss-ckbi-probe/install-and-run.sh" >/dev/null; then
    echo "nss-ckbi-probe must not replace the shared seed" >&2
    exit 1
fi
echo "nss ckbi probe requires GreD roots before Firefox online: PASS"
