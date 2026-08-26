#!/usr/bin/env bash
# M1 — reject missing/invalid Bearer; accept configured key.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${AUTH_BEARER_CFG:-$ROOT/packages/li-net-httpd/examples/auth_bearer.toml}"
CONF="/tmp/httpd-auth-bearer.conf"
PUBLIC="$(cd "$ROOT/packages/li-net-httpd/examples" && mkdir -p public && pwd)/public"

if [[ ! -x "$HTTPD" ]]; then
  echo "test-auth-bearer: build li-httpd first" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"

chmod +x "$ROOT/scripts/lic-validate-httpd-config.sh"
"$ROOT/scripts/lic-validate-httpd-config.sh" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"

FRONT_PORT="$(grep '^listen_port=' "$CONF" | cut -d= -f2)"
fuser -k "${FRONT_PORT}/tcp" 2>/dev/null || true
sleep 0.2

timeout 15 "$HTTPD" "$CONF" >/dev/null 2>&1 &
FE_PID=$!
sleep 0.5

code_no=$(curl -s -m 3 -o /dev/null -w "%{http_code}" "http://127.0.0.1:${FRONT_PORT}/index.html" || echo "000")
code_bad=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -H "Authorization: Bearer wrong-key" \
  "http://127.0.0.1:${FRONT_PORT}/index.html" || echo "000")
code_ok=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -H "Authorization: Bearer dev-agent-key" \
  "http://127.0.0.1:${FRONT_PORT}/index.html" || echo "000")

kill "$FE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true

if [[ "$code_no" != "401" || "$code_bad" != "401" || "$code_ok" != "200" ]]; then
  echo "test-auth-bearer: FAIL no=$code_no bad=$code_bad ok=$code_ok (front=$FRONT_PORT)" >&2
  exit 1
fi
echo "test-auth-bearer: ok (401 without key; 200 with dev-agent-key)"
