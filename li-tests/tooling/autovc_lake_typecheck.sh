#!/usr/bin/env bash
# G-lean: generated AutoVC.lean must compile in docs/semantics (LiArray surface).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
if ! command -v lake >/dev/null 2>&1; then
  echo "autovc_lake_typecheck: skipped (lake not installed)"
  exit 0
fi
SAMPLE="$ROOT/li-tests/contracts_verify/linalg_dot4_int_closed.li"
rm -f "$ROOT/build/generated/AutoVC.lean"
"$LIC" build "$SAMPLE" -o /dev/null
test -f "$ROOT/build/generated/AutoVC.lean"
(cd "$ROOT/docs/semantics" && lake build AutoVC Discharge)
echo "autovc_lake_typecheck: ok"
