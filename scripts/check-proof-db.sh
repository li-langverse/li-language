#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/lib/li-ui.sh"
if [[ "${PROOF_DB_SKIP:-0}" == "1" ]]; then
  li_warn "proof-db gate skipped (PROOF_DB_SKIP=1)"
  exit 0
fi
STRICT="${LI_PROOF_DB_STRICT:-0}"
BASELINE="${LI_PROOF_DB_BASELINE:-$ROOT/proof-db/baseline.jsonl}"
li_phase "proof-db release gate (strict=${STRICT})"
if [[ ! -f "$BASELINE" ]]; then
  if [[ "$STRICT" == "1" ]]; then
    li_gate_fail "missing baseline $BASELINE"
    exit 1
  fi
  li_warn "proof-db: missing baseline (advisory)"
  exit 0
fi
if [[ ! -f "$ROOT/build/generated/AutoVC.lean" ]]; then
  li_warn "proof-db: AutoVC.lean missing — N/A for AutoVC rows"
fi
export BASELINE STRICT ROOT
if ! python3 "$ROOT/scripts/check-proof-db.py"; then
  if [[ "$STRICT" == "1" ]]; then
    li_gate_fail "proof-db drift"
    exit 1
  fi
  li_warn "proof-db advisory drift"
fi
li_gate_ok "proof-db release gate"
