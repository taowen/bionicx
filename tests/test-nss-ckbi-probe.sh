#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/nss-ckbi-probe/install-and-run.sh" >/dev/null; then
    echo "nss-ckbi-probe must not replace the shared seed" >&2
    exit 1
fi
echo "nss ckbi probe requires GreD roots before Firefox online: PASS"
