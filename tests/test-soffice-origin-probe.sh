#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if grep -F -- '--runtime-root' \
        "$repo_dir/examples/soffice-origin-probe/install-and-run.sh" >/dev/null; then
    echo "soffice-origin-probe must not replace the shared seed" >&2
    exit 1
fi
chmod +x "$repo_dir/examples/soffice-origin-probe/install-and-run.sh"
echo "soffice origin probe covers multiarch symlink \$ORIGIN: PASS"
