#!/usr/bin/env bash
set -euo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
chmod +x "$R/scripts/proof-db-report.sh"
if "$R/scripts/proof-db-report.sh" --baseline "$R/proof-db/expected.json" --run "$R/proof-db/examples/sample-run.jsonl" >/dev/null 2>&1; then
  echo "FAIL: expected non-zero exit"; exit 1
fi
"$R/scripts/proof-db-report.sh" --baseline "$R/proof-db/expected.json" --run "$R/proof-db/examples/sample-run.jsonl"   --allow-discrepancies --format both --out "$T/b" >/dev/null
test -f "$T/b/report.md" -a -f "$T/b/report.html"
"$R/scripts/proof-db-report.sh" --a "$R/proof-db/examples/sample-run.jsonl" --b "$R/proof-db/examples/sample-run-b.jsonl"   --out "$T/ab" --allow-discrepancies >/dev/null
grep -q run_delta "$T/ab/report.md"
echo proof_db_report_smoke: ok
