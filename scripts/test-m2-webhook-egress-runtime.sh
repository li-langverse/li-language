#!/usr/bin/env bash
# m2-webhook-egress-runtime: webhook SSRF allowlist enforced on X-Li-Webhook-Url.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${WEBHOOK_CFG:-$ROOT/packages/li-net-httpd/examples/webhook_egress.toml}"
CONF="/tmp/httpd-webhook.conf"
PUBLIC="/tmp/httpd-webhook-public"
BE_PORT=18171
FRONT_PORT=18170

if [[ ! -x "$HTTPD" ]]; then
  echo "test-m2-webhook-egress-runtime: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q 'm2_webhook_allow=https://hooks.example.com/v1/callback' "$CONF"

fuser -k "${BE_PORT}/tcp" "${FRONT_PORT}/tcp" 2>/dev/null || true
sleep 0.3

python3 -u - "$BE_PORT" <<'PY' >/dev/null 2>&1 &
import socket
import sys
import threading

port = int(sys.argv[1])


def read_request(conn: socket.socket) -> bytes:
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            return b""
        data += chunk
    return data


def handle(conn: socket.socket) -> None:
    try:
        read_request(conn)
        conn.sendall(
            b"HTTP/1.1 200 OK\r\n"
            b"Content-Length: 2\r\n"
            b"Content-Type: text/plain\r\n"
            b"Connection: close\r\n"
            b"\r\n"
            b"ok"
        )
    except OSError:
        pass
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

LI_HTTPD_WORKERS=1 "$HTTPD" "$CONF" >/dev/null 2>&1 &
FE_PID=$!
sleep 1.0

fail=0

bad_code=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -X POST "http://127.0.0.1:${FRONT_PORT}/dispatch" \
  -H "Content-Type: application/json" \
  -H "X-Li-Webhook-Url: https://127.0.0.1/evil" \
  -d '{}' 2>/dev/null || echo "000")
if [[ "$bad_code" != "403" ]]; then
  echo "test-m2-webhook-egress-runtime: FAIL blocked URL expected 403 got $bad_code" >&2
  fail=1
fi

good_code=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -X POST "http://127.0.0.1:${FRONT_PORT}/dispatch" \
  -H "Content-Type: application/json" \
  -H "X-Li-Webhook-Url: https://hooks.example.com/v1/callback" \
  -d '{}' 2>/dev/null || echo "000")
if [[ "$good_code" != "200" ]]; then
  echo "test-m2-webhook-egress-runtime: FAIL allowed URL expected 200 got $good_code" >&2
  fail=1
fi

kill "$FE_PID" "$BE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "test-m2-webhook-egress-runtime: ok (403 blocked, 200 allowed)"
