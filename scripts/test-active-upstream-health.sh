#!/usr/bin/env bash
# M1 active health — periodic GET probes mark dead peers down; live peer serves traffic.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${ACTIVE_HEALTH_CFG:-$ROOT/packages/li-net-httpd/examples/active_health.toml}"
BE_PORT=18101
CONF="/tmp/httpd-active-health.conf"
PUBLIC="$(cd "$ROOT/packages/li-net-httpd/examples" && mkdir -p public && pwd)/public"

if [[ ! -x "$HTTPD" ]]; then
  echo "test-active-upstream-health: build li-httpd first" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q '^health_active=1' "$CONF"
grep -q '^health_active_path=/' "$CONF"

fuser -k "${BE_PORT}/tcp" 18100/tcp 18102/tcp 2>/dev/null || true
sleep 0.3

# Only BE_PORT listens; 18102 is dead — active probes should mark it down before client request.
timeout 30 "$HTTPD" "$BE_PORT" "$PUBLIC" >/dev/null 2>&1 &
BE_PID=$!
sleep 0.4

FRONT_PORT="$(grep '^listen_port=' "$CONF" | cut -d= -f2)"
timeout 25 "$HTTPD" "$CONF" >/dev/null 2>&1 &
FE_PID=$!
sleep 2

code=$(curl -s -m 3 -o /dev/null -w "%{http_code}" "http://127.0.0.1:${FRONT_PORT}/" || echo "000")

kill "$FE_PID" "$BE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true

if [[ "$code" != "200" ]]; then
  echo "test-active-upstream-health: FAIL expected 200 got $code (front=$FRONT_PORT)" >&2
  exit 1
fi
echo "test-active-upstream-health: ok (active probe skipped dead peer 18102)"
