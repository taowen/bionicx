#!/usr/bin/env bash
set -euo pipefail
printf 'Version needs section: Name: GLIBC_%s\n' "${MOCK_GLIBC_VERSION:?}"
