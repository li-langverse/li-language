#!/usr/bin/env bash
# Phase 2e: generated AutoVC must carry real Props (not bare True stubs) for contracts_verify.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
VC="$ROOT/build/generated/AutoVC.lean"
SAMPLE="$ROOT/li-tests/contracts_verify/sqrt_contract.li"

"$LIC" build "$SAMPLE" -o /dev/null
test -f "$VC"

grep -q 'vc_sqrt_pos_requires_0' "$VC"
grep -q '≥' "$VC" || grep -q '>=' "$VC"
if grep -q 'vc_sqrt_pos_requires_0.*: Prop := True' "$VC"; then
  echo "vc_emit_contracts: requires x >= 0.0 must not lower to True"
  exit 1
fi

grep -q 'ensures_1_proved\|result ==' "$VC" || grep -q 'Float.abs' "$VC" || grep -q 'opaque' "$VC"

echo "vc_emit_contracts: ok"
