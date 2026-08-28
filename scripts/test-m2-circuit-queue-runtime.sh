#!/usr/bin/env bash
# m2-circuit-queue-runtime: live li-httpd queue 429 + Retry-After; circuit opens when peers fail.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${CIRCUIT_QUEUE_CFG:-$ROOT/packages/li-net-httpd/examples/circuit_queue.toml}"
CONF="/tmp/httpd-circuit-queue.conf"
PUBLIC="/tmp/httpd-circuit-queue-public"
BE_PORT=18151
FRONT_PORT=18150

if [[ ! -x "$HTTPD" ]]; then
  echo "test-m2-circuit-queue-runtime: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q '^m2_enabled=1' "$CONF"
grep -q '^m2_queue_max_depth=1' "$CONF"
grep -q '^m2_queue_retry_after_sec=3' "$CONF"
grep -q '^m2_cb_error_threshold=2' "$CONF"

fuser -k "${BE_PORT}/tcp" "${FRONT_PORT}/tcp" 2>/dev/null || true
sleep 0.3

python3 -u - "$BE_PORT" <<'PY' >/dev/null 2>&1 &
import socket
import sys
import threading
import time

port = int(sys.argv[1])


def read_request(conn: socket.socket) -> bytes:
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            return b""
        data += chunk
    return data


def handle_slow(conn: socket.socket) -> None:
    time.sleep(8)
    conn.sendall(
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 2\r\n"
        b"Content-Type: text/plain\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"ok"
    )
    conn.close()


def handle(conn: socket.socket) -> None:
    try:
        req = read_request(conn)
        if not req:
            return
        if b"/slow" in req:
            handle_slow(conn)
        else:
            conn.sendall(
                b"HTTP/1.1 200 OK\r\n"
                b"Content-Length: 2\r\n"
                b"Content-Type: text/plain\r\n"
                b"Connection: close\r\n"
                b"\r\n"
                b"ok"
            )
            conn.close()
    except OSError:
        pass


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

# Queue depth: one in-flight proxy holds the slot; second request gets 429 + Retry-After: 3.
curl -s -m 12 -o /tmp/httpd-m2-queue-hold.out -w "%{http_code}" \
  "http://127.0.0.1:${FRONT_PORT}/slow" >/tmp/httpd-m2-queue-hold.code 2>/dev/null &
HOLD_PID=$!
sleep 0.4
queue_code=$(curl -s -m 3 -D /tmp/httpd-m2-queue-hdr.out -o /dev/null -w "%{http_code}" \
  "http://127.0.0.1:${FRONT_PORT}/queued" 2>/dev/null || echo "000")
queue_code="${queue_code:-000}"
retry_hdr=$(grep -i '^Retry-After:' /tmp/httpd-m2-queue-hdr.out 2>/dev/null | tr -d '\r' || true)
kill "$HOLD_PID" 2>/dev/null || true
wait "$HOLD_PID" 2>/dev/null || true

if [[ "$queue_code" != "429" ]]; then
  echo "test-m2-circuit-queue-runtime: FAIL queue expected 429 got $queue_code" >&2
  fail=1
fi
if [[ "$retry_hdr" != *"3"* ]]; then
  echo "test-m2-circuit-queue-runtime: FAIL queue missing Retry-After: 3 (got: ${retry_hdr:-none})" >&2
  fail=1
fi

# Circuit breaker: upstream down — failures open circuit; next client gets 429 + Retry-After.
kill "$BE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true
sleep 0.3
fuser -k "${BE_PORT}/tcp" 2>/dev/null || true
sleep 0.3

for _ in 1 2; do
  curl -s -m 2 -o /dev/null "http://127.0.0.1:${FRONT_PORT}/trip" 2>/dev/null || true
  sleep 0.15
done
cb_code=$(curl -s -m 3 -D /tmp/httpd-m2-cb-hdr.out -o /dev/null -w "%{http_code}" \
  "http://127.0.0.1:${FRONT_PORT}/trip" 2>/dev/null || echo "000")
cb_code="${cb_code:-000}"
cb_retry=$(grep -i '^Retry-After:' /tmp/httpd-m2-cb-hdr.out 2>/dev/null | tr -d '\r' || true)

if [[ "$cb_code" != "429" ]]; then
  echo "test-m2-circuit-queue-runtime: FAIL circuit expected 429 got $cb_code" >&2
  fail=1
fi
if [[ "$cb_retry" != *"3"* ]]; then
  echo "test-m2-circuit-queue-runtime: FAIL circuit missing Retry-After: 3 (got: ${cb_retry:-none})" >&2
  fail=1
fi

kill "$FE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "test-m2-circuit-queue-runtime: ok (queue 429 + Retry-After, circuit-open 429)"
