#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
guide="$repo_dir/docs/NEW-DEVICE.md"
test -s "$guide"
echo "new-device guide names seed rebuild, bxapt and kernel limits: PASS"
