#!/usr/bin/env bash
# m15-leak-censor-runtime: live li-httpd proxy redacts sk- secrets on upstream egress (Tier G).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${LEAK_CENSOR_CFG:-$ROOT/packages/li-net-httpd/examples/leak_censor_proxy.toml}"
CONF="/tmp/httpd-leak-censor.conf"
PUBLIC="$(cd "$ROOT/packages/li-net-httpd/examples" && mkdir -p public && pwd)/public"
BE_PORT=18161
FRONT_PORT=18160
SECRET="sk-leaksecret99"

if [[ ! -x "$HTTPD" ]]; then
  echo "test-m15-leak-censor-runtime: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q '^listen_port=18160' "$CONF"
grep -q '^leak_censor_enabled=1' "$CONF"
grep -q '^leak_censor_pattern=openai_sk' "$CONF"
grep -q '^upstream_peer=18161' "$CONF"

fuser -k "${BE_PORT}/tcp" "${FRONT_PORT}/tcp" 2>/dev/null || true
pkill -f 'httpd-leak-censor' 2>/dev/null || true
sleep 0.5

python3 -u - "$BE_PORT" "$SECRET" <<'PY' >/dev/null 2>&1 &
import json
import socket
import sys
import threading

port = int(sys.argv[1])
secret = sys.argv[2]


def read_request(conn: socket.socket) -> bytes:
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            return b""
        data += chunk
    return data


def handle_json(conn: socket.socket) -> None:
    body = json.dumps({"api_key": secret, "note": "upstream leak"}).encode()
    conn.sendall(
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Type: application/json\r\n"
        b"Content-Length: " + str(len(body)).encode() + b"\r\n"
        b"Connection: close\r\n"
        b"\r\n" + body
    )


def handle_sse(conn: socket.socket) -> None:
    payload = f'data: {{"k":"{secret}"}}\n\n'.encode()
    conn.sendall(
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Type: text/event-stream\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )
    chunk = f"{len(payload):x}\r\n".encode() + payload + b"\r\n0\r\n\r\n"
    conn.sendall(chunk)


def handle(conn: socket.socket) -> None:
    try:
        req = read_request(conn)
        if not req:
            return
        if b"text/event-stream" in req.lower():
            handle_sse(conn)
        else:
            handle_json(conn)
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
sleep 2.0

json_body=$(curl -s -m 5 \
  -X POST "http://127.0.0.1:${FRONT_PORT}/v1/leak-json" \
  -H "Content-Type: application/json" \
  -d '{}' || true)

sse_body=$(curl -s -N -m 5 \
  -X POST "http://127.0.0.1:${FRONT_PORT}/v1/leak-sse" \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -d '{}' || true)

kill "$FE_PID" "$BE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true

fail=0
if [[ -z "$json_body" ]]; then
  echo "test-m15-leak-censor-runtime: FAIL empty JSON proxy response" >&2
  fail=1
elif [[ "$json_body" == *"$SECRET"* ]]; then
  echo "test-m15-leak-censor-runtime: FAIL JSON egress still contains secret" >&2
  fail=1
elif [[ "$json_body" != *"[REDACTED]"* ]]; then
  echo "test-m15-leak-censor-runtime: FAIL JSON egress missing [REDACTED]" >&2
  fail=1
fi

if [[ -z "$sse_body" ]]; then
  echo "test-m15-leak-censor-runtime: FAIL empty SSE proxy response" >&2
  fail=1
elif [[ "$sse_body" == *"$SECRET"* ]]; then
  echo "test-m15-leak-censor-runtime: FAIL SSE egress still contains secret" >&2
  fail=1
elif [[ "$sse_body" != *"[REDACTED]"* ]]; then
  echo "test-m15-leak-censor-runtime: FAIL SSE egress missing [REDACTED]" >&2
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "test-m15-leak-censor-runtime: ok (JSON + SSE proxy redact sk- on egress)"
