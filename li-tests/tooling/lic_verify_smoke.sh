#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
SAMPLE="$ROOT/li-tests/decorators/parallel_with_disjoint.li"
OUT="$("$LIC" verify "$SAMPLE" 2>&1)"
echo "$OUT"
echo "$OUT" | grep -q 'requires='
"$LIC" verify "$SAMPLE" --lean >/dev/null
echo "lic_verify_smoke: ok"
