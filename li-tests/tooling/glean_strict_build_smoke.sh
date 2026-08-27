#!/usr/bin/env bash
# Smoke: lic build --strict-lean on a closed-contract specimen (2f / G-lean path).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
SAMPLE="$ROOT/li-tests/contracts_verify/linalg_dot4_int_closed.li"
"$LIC" build "$SAMPLE" --strict-lean -o /dev/null
echo "glean_strict_build_smoke: ok"
