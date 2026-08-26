#!/usr/bin/env bash
# tier5_http nginx-src-audit gate (read-only checklist; no live nginx build).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONPATH="$ROOT/benchmarks/harness${PYTHONPATH:+:$PYTHONPATH}"
HARNESS="$ROOT/benchmarks/harness"
TIER5="$ROOT/benchmarks/tier5_http"

echo "== nginx_mitigations.toml present =="
test -f "$TIER5/nginx_mitigations.toml"

echo "== CHANGES fixture (submodule may omit CHANGES on GitHub mirror) =="
test -f "$TIER5/fixtures/nginx-1.26.2-CHANGES.txt"

if [[ -f "$ROOT/.gitmodules" ]] && grep -q 'third_party/nginx' "$ROOT/.gitmodules"; then
  echo "== .gitmodules nginx submodule registered =="
  grep -q 'third_party/nginx' "$ROOT/.gitmodules"
fi

chmod +x "$HARNESS/audit_nginx_src.py" 2>/dev/null || true

echo "== audit_nginx_src.py --check =="
python3 "$HARNESS/audit_nginx_src.py" --check

echo "== audit_nginx_src.py --report =="
python3 "$HARNESS/audit_nginx_src.py" --report

echo "check-tier5-nginx-src-audit: OK"
