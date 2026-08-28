#!/usr/bin/env bash
# m1-serve-production: long-lived li-httpd — workers, keep-alive, static (argv), proxy (.conf).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${SERVE_PRODUCTION_CFG:-$ROOT/packages/li-net-httpd/examples/serve_production.toml}"
CONF="/tmp/httpd-serve-production.conf"
PUBLIC="$(cd "$ROOT/packages/li-net-httpd/examples" && mkdir -p public && pwd)/public"
BE_PORT=18111
STATIC_PORT=18112
FRONT_PORT=18110

if [[ ! -x "$HTTPD" ]]; then
  echo "test-serve-production: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo static-ok > "$PUBLIC/index.html"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q '^workers=2' "$CONF"
grep -q '^listen_port=18110' "$CONF"

fuser -k "${BE_PORT}/tcp" "${STATIC_PORT}/tcp" "${FRONT_PORT}/tcp" 2>/dev/null || true
sleep 0.3

# Static + keep-alive on argv production path (2 workers via LI_HTTPD_WORKERS).
LI_HTTPD_WORKERS=2 timeout 20 "$HTTPD" "$STATIC_PORT" "$PUBLIC" >/dev/null 2>&1 &
ST_PID=$!
sleep 0.8

static_code=$(curl -s -m 3 -o /dev/null -w "%{http_code}" "http://127.0.0.1:${STATIC_PORT}/index.html" || echo "000")
keep_resp=$(printf 'GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\nGET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n' \
  | timeout 3 nc -w 2 127.0.0.1 "${STATIC_PORT}" 2>/dev/null || true)
keep_hits=$(echo "$keep_resp" | grep -c 'HTTP/1.1 200' || true)

kill "$ST_PID" 2>/dev/null || true
wait "$ST_PID" 2>/dev/null || true

if [[ "$static_code" != "200" || "${keep_hits:-0}" -lt 2 ]]; then
  echo "test-serve-production: FAIL static=$static_code keep_alive_hits=${keep_hits:-0}" >&2
  exit 1
fi

timeout 30 "$HTTPD" "$BE_PORT" "$PUBLIC" >/dev/null 2>&1 &
BE_PID=$!
sleep 0.4

timeout 25 "$HTTPD" "$CONF" >/dev/null 2>&1 &
FE_PID=$!
sleep 1.2

worker_n="$(pgrep -cf '[/]li-httpd.*httpd-serve-production.conf' 2>/dev/null || true)"
if [[ -z "$worker_n" || "$worker_n" -lt 2 ]]; then
  worker_n="$(pgrep -cf "$HTTPD" 2>/dev/null || true)"
fi
if [[ -z "$worker_n" || "$worker_n" -lt 2 ]]; then
  echo "test-serve-production: FAIL expected >=2 li-httpd workers, saw ${worker_n:-0}" >&2
  kill "$FE_PID" "$BE_PID" 2>/dev/null || true
  exit 1
fi

proxy_code=$(curl -s -m 3 -o /dev/null -w "%{http_code}" "http://127.0.0.1:${FRONT_PORT}/" || echo "000")

kill "$FE_PID" "$BE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true

if [[ "$proxy_code" != "200" ]]; then
  echo "test-serve-production: FAIL proxy=$proxy_code" >&2
  exit 1
fi
echo "test-serve-production: ok (workers=${worker_n}, static+keep-alive, proxy=200)"
