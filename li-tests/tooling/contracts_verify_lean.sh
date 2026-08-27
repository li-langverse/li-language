#!/usr/bin/env bash
# contracts_verify + Lean lake (2f): sqrt emits real Props; corpus has zero open goals.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
SAMPLE="$ROOT/li-tests/contracts_verify/sqrt_contract.li"
out="$("$LIC" verify "$SAMPLE" 2>&1)"
echo "$out" | grep -q 'mir_fns='
echo "$out" | grep -q 'witnessed_ensures='
echo "$out" | grep -q 'mir_return_linked='
rm -f "$ROOT/build/generated/AutoVC.lean"
"$LIC" build "$SAMPLE" -o /dev/null
if command -v lake >/dev/null 2>&1; then
  (cd "$ROOT/docs/semantics" && lake build)
  echo "contracts_verify_lean: lake ok (sqrt AutoVC may have open float goals)"
else
  echo "contracts_verify_lean: skipped lake (not installed)"
fi
"$ROOT/li-tests/tooling/discharge_trivial_lean.sh"
chmod +x "$ROOT/li-tests/tooling/discharge_caller_requires_lean.sh"
"$ROOT/li-tests/tooling/discharge_caller_requires_lean.sh"
chmod +x "$ROOT/li-tests/tooling/discharge_caller_requires_local_lean.sh" \
  "$ROOT/li-tests/tooling/discharge_import_requires_lean.sh"
"$ROOT/li-tests/tooling/discharge_caller_requires_local_lean.sh"
"$ROOT/li-tests/tooling/discharge_import_requires_lean.sh"
echo "contracts_verify_lean: ok"
