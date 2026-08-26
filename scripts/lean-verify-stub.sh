#!/usr/bin/env bash
# Phase 2f: `lake build` on docs/semantics + optional AutoVC open-goal check.
# Invoked from lic only; use --check-open-goals (not env) for strict open-VC pass.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SEM="$ROOT/docs/semantics"
CHECK_OPEN=0
for arg in "$@"; do
  case "$arg" in
    --check-open-goals) CHECK_OPEN=1 ;;
  esac
done
if [[ -x "$(command -v lake)" && -f "$SEM/lakefile.lean" ]]; then
  echo "lean: running lake in docs/semantics"
  (cd "$SEM" && lake build) || exit $?
  if [[ -f "$ROOT/build/generated/AutoVC.lean" ]]; then
    echo "lean: typechecking generated AutoVC"
    (cd "$SEM" && lake build AutoVC Discharge) || exit $?
  fi
  if [[ "$CHECK_OPEN" == 1 && -f "$ROOT/build/generated/AutoVC.lean" ]]; then
    chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
    "$ROOT/scripts/check-autovc-open-goals.sh"
  fi
  exit 0
fi
echo "lean: skipped (install Lean 4 + lake; see docs/verification/provability-gaps.md G-lean)"
exit 0
