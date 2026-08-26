#!/usr/bin/env bash
# m1-upstream-keepalive: pooled upstream fds survive backend idle close (no stale 502s).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${UPSTREAM_KEEPALIVE_CFG:-$ROOT/packages/li-net-httpd/examples/upstream_keepalive.toml}"
CONF="/tmp/httpd-upstream-keepalive.conf"
PUBLIC="$(cd "$ROOT/packages/li-net-httpd/examples" && mkdir -p public && pwd)/public"
BE_PORT=18121
FRONT_PORT=18120
REQUESTS="${UPSTREAM_KEEPALIVE_REQUESTS:-40}"

if [[ ! -x "$HTTPD" ]]; then
  echo "test-upstream-keepalive: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q '^listen_port=18120' "$CONF"
grep -q '^upstream_peer=18121' "$CONF"

fuser -k "${BE_PORT}/tcp" "${FRONT_PORT}/tcp" 2>/dev/null || true
sleep 0.3

# Backend closes each connection after responding (keep-alive header, RST on reuse).
python3 -u - "$BE_PORT" <<'PY' >/dev/null 2>&1 &
import socket
import sys
import threading

port = int(sys.argv[1])

def handle(conn: socket.socket) -> None:
    try:
        data = b""
        while b"\r\n\r\n" not in data:
            chunk = conn.recv(4096)
            if not chunk:
                return
            data += chunk
        conn.sendall(
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Length: 2\r\n"
            b"Content-Type: text/plain\r\n"
            b"Connection: keep-alive\r\n"
            b"\r\n"
            b"ok"
        )
    finally:
        conn.close()

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", port))
srv.listen(64)
while True:
    c, _ = srv.accept()
    threading.Thread(target=handle, args=(c,), daemon=True).start()
PY
BE_PID=$!
sleep 0.5

timeout 30 "$HTTPD" "$CONF" >/dev/null 2>&1 &
FE_PID=$!
sleep 1.0

fail=0
for i in $(seq 1 "$REQUESTS"); do
  code=$(curl -s -m 3 -o /dev/null -w "%{http_code}" "http://127.0.0.1:${FRONT_PORT}/probe-${i}" || echo "000")
  if [[ "$code" != "200" ]]; then
    echo "test-upstream-keepalive: FAIL request $i expected 200 got $code" >&2
    fail=1
    break
  fi
done

kill "$FE_PID" "$BE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "test-upstream-keepalive: ok (${REQUESTS} proxy requests, no stale 502)"
