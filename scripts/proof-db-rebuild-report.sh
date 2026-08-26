#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
source "$ROOT/scripts/lib/li-ui.sh"
BASELINE="${LI_PROOF_DB_BASELINE:-$ROOT/proof-db/baseline.json}"
STRICT="${LI_PROOF_DB_STRICT:-0}"
chmod +x "$ROOT/scripts/export-proof-db.sh"
[[ -f "$BASELINE" ]] || { li_fail "missing $BASELINE"; exit 1; }
[[ -f "$ROOT/build/generated/AutoVC.lean" ]] || { li_warn "no AutoVC"; [[ "$STRICT" == "1" ]] && exit 1; exit 0; }
cur="$(mktemp)"; trap 'rm -f "$cur"' EXIT
"$ROOT/scripts/export-proof-db.sh" >"$cur"
export BASELINE CURRENT="$cur" STRICT ROOT
python3 - <<'PY'
import json, os, sys
from pathlib import Path
bp, cp = Path(os.environ["BASELINE"]), Path(os.environ["CURRENT"])
strict = os.environ.get("STRICT") == "1"
data = json.loads(bp.read_text())
base = {r["id"]: r for r in data.get("records", [])}
cur = {json.loads(l)["id"]: json.loads(l) for l in cp.read_text().splitlines() if l.strip()}
reg = []
for pid, b in sorted(base.items()):
    if b.get("status") != "proved": continue
    c = cur.get(pid)
    if not c: reg.append(f"{pid}: missing")
    elif c.get("status") != "proved": reg.append(f"{pid}: {c.get('status')}")
pb = sum(1 for r in base.values() if r.get("status") == "proved")
pc = sum(1 for r in cur.values() if r.get("status") == "proved")
print("# proof-db rebuild report\n")
print(f"- Baseline pin: {data.get('release_pin','—')}")
print(f"- Proved: {pc} current / {pb} baseline ({len(cur)} scanned)")
for m in reg: print(f"  - regression: {m}")
if not reg: print("- Regressions: none")
elif not strict: print("- Mode: advisory (LI_PROOF_DB_STRICT=1 to fail)")
sys.exit(1 if reg and strict else 0)
PY
