#!/usr/bin/env bash
# Smoke: lic JSON diagnostics for agents (Vision-LLM / agent-first).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
BAD="$ROOT/li-tests/typecheck/bad_array_index.li"

out="$("$LIC" check --format=json "$BAD" 2>/dev/null || true)"
echo "$out" | grep -q '"ok":false'
echo "$out" | grep -q '"schema":"diagnostic-v1"'
echo "$out" | grep -q '"code":"E0201"'
echo "$out" | grep -q 'bad_array_index.li'

diag="$("$LIC" diagnose "$BAD" 2>/dev/null || true)"
echo "$diag" | grep -q '"command":"diagnose"'
echo "$diag" | grep -q '"diagnostics":\['

ok="$("$LIC" check --format=json "$ROOT/li-tests/lexer_parser/fib.li" 2>/dev/null)"
echo "$ok" | grep -q '"ok":true'
echo "$ok" | grep -q '"diagnostics":\[\]'

if command -v jq >/dev/null 2>&1; then
  echo "$out" | "$ROOT/scripts/lic-fix-suggest.sh" >/dev/null || true
fi

echo "diagnose_json_smoke: ok"
