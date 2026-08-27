#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/lib/li-ui.sh"
[[ "${PROOF_DB_SKIP:-0}" == "1" ]] && { li_gate_ok "proof-db discrepancies skipped"; exit 0; }
li_phase "proof-db discrepancies"
python3 "$ROOT/scripts/proof-db/compare_reference.py" --write --min-count 5
li_gate_ok "proof-db discrepancies"
