#!/usr/bin/env bash
# m3-optional: live li-httpd rejects over-cap and zero x-token-budget with 429.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${M3_TOKEN_BUDGET_CFG:-$ROOT/packages/li-net-httpd/examples/m3_token_budget.toml}"
CONF="/tmp/httpd-m3-token-budget.conf"
PUBLIC="$(cd "$ROOT/packages/li-net-httpd/examples" && mkdir -p public && pwd)/public"

if [[ ! -x "$HTTPD" ]]; then
  echo "test-m3-token-budget-runtime: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/health"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q '^m3_token_budget_enabled=1' "$CONF"
grep -q '^m3_token_budget_max=5000' "$CONF"

FRONT_PORT="$(grep '^listen_port=' "$CONF" | cut -d= -f2)"
fuser -k "${FRONT_PORT}/tcp" 2>/dev/null || true
sleep 0.2

timeout 15 "$HTTPD" "$CONF" >/dev/null 2>&1 &
FE_PID=$!
sleep 0.5

base="http://127.0.0.1:${FRONT_PORT}/health"
code_ok=$(curl -s -m 3 -o /dev/null -w "%{http_code}" "$base" || echo "000")
code_in=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -H "x-token-budget: 1000" "$base" || echo "000")
code_over=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -H "x-token-budget: 999999" "$base" || echo "000")
code_zero=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -H "x-token-budget: 0" "$base" || echo "000")

kill "$FE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true

if [[ "$code_ok" != "200" || "$code_in" != "200" || "$code_over" != "429" || "$code_zero" != "429" ]]; then
  echo "test-m3-token-budget-runtime: FAIL ok=$code_ok in=$code_in over=$code_over zero=$code_zero" >&2
  exit 1
fi
echo "test-m3-token-budget-runtime: ok (200 within cap; 429 over-cap and zero)"
