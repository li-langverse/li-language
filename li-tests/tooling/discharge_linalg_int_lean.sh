#!/usr/bin/env bash
# P-linalg (2f): closed int/float-index linalg contracts → zero open Prop goals.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
for sample in \
  "$ROOT/li-tests/contracts_verify/linalg_dot4_int_closed.li" \
  "$ROOT/li-tests/contracts_verify/linalg_sum4_int_closed.li" \
  "$ROOT/li-tests/contracts_verify/linalg_mat2_entry00_int_closed.li" \
  "$ROOT/li-tests/contracts_verify/linalg_norm4_int_closed.li" \
  "$ROOT/li-tests/contracts_verify/linalg_axpy4_int_closed.li" \
  "$ROOT/li-tests/contracts_verify/linalg_dot4_float_closed.li" \
  "$ROOT/li-tests/contracts_verify/linalg_mat2_at2_float_closed.li" \
  "$ROOT/li-tests/contracts_verify/linalg_dot4_int_loop_open.li" \
  "$ROOT/li-tests/contracts_verify/linalg_mat2_callproc_float_closed.li"; do
  AUTOVC="$ROOT/build/generated/AutoVC.lean"
  rm -f "$AUTOVC"
  "$LIC" build "$sample" -o /dev/null
  test -f "$AUTOVC"
  grep -qE 'Phase 2f: (return expression matches ensures|fixed-bound dot loop witness|prelude dot)|Li\.Discharge\.mat2_at2' \
    "$AUTOVC" || {
    echo "discharge_linalg_int_lean: missing witness marker in $sample"
    exit 1
  }
  "$ROOT/scripts/check-autovc-open-goals.sh" "$AUTOVC"
done
if command -v lake >/dev/null 2>&1; then
  (cd "$ROOT/docs/semantics" && lake build)
  echo "discharge_linalg_int_lean: lake ok"
else
  echo "discharge_linalg_int_lean: skipped lake (not installed)"
fi
echo "discharge_linalg_int_lean: ok"
