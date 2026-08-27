#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/lib/li-ui.sh"
export ROOT
export CATALOG="$ROOT/proof-db/math/lemmas/catalog.json"
export ENTRIES="$ROOT/docs/verification/proof-database/entries/math-lemmas.toml"
export OUT="${LI_PROOF_DB_GAP2_OUT:-$ROOT/data/proof-db/gap2-report.md}"
SKIP=0
while [[ $# -gt 0 ]]; do
  case $1 in
    --skip-rebuild) SKIP=1; shift ;;
    --out) OUT="$2"; export OUT; shift 2 ;;
    *) shift ;;
  esac
done
[[ -f "$CATALOG" && -f "$ENTRIES" ]] || { li_fail "missing proof-db inputs"; exit 1; }
if [[ "$SKIP" != 1 ]]; then
  "$ROOT/scripts/proof-db/rebuild_lemmas.sh" --skip-build || li_warn "rebuild_lemmas advisory"
fi
python3 "$ROOT/scripts/proof-db/_gap2_report.py"
li_gate_ok "proof-db gap2 report"
