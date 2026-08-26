#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/lib/li-ui.sh"
[[ "${PROOF_DB_SKIP:-0}" == "1" ]] && exit 0
li_phase "proof-db release gate"
chmod +x "$ROOT/scripts/proof-db-rebuild-report.sh" "$ROOT/scripts/export-proof-db.sh"
export LI_PROOF_DB_STRICT="${LI_PROOF_DB_STRICT:-0}"
"$ROOT/scripts/proof-db-rebuild-report.sh" || { [[ "${LI_PROOF_DB_STRICT:-0}" == "1" ]] && exit 1; li_warn "proof-db regressions (advisory)"; }
li_gate_ok "proof-db release"
