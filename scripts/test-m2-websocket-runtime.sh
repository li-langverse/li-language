#!/usr/bin/env bash
# m2-websocket-runtime: WebSocket upgrade + bidirectional proxy on live li-httpd.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${WS_CFG:-$ROOT/packages/li-net-httpd/examples/websocket_proxy.toml}"
CONF="/tmp/httpd-ws.conf"
PUBLIC="/tmp/httpd-ws-public"
BE_PORT=18161
FRONT_PORT=18160

if [[ ! -x "$HTTPD" ]]; then
  echo "test-m2-websocket-runtime: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q 'route_require=GET|/ws/tools|websocket' "$CONF"

fuser -k "${BE_PORT}/tcp" "${FRONT_PORT}/tcp" 2>/dev/null || true
sleep 0.3

python3 -u - "$BE_PORT" <<'PY' >/dev/null 2>&1 &
import base64
import hashlib
import re
import socket
import sys
import threading

port = int(sys.argv[1])
MAGIC = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def read_headers(conn: socket.socket) -> bytes:
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            return b""
        data += chunk
    return data


def handle(conn: socket.socket) -> None:
    try:
        req = read_headers(conn)
        if not req:
            return
        m = re.search(br"Sec-WebSocket-Key:\s*(\S+)", req, re.I)
        key = m.group(1) if m else b"test"
        accept = base64.b64encode(hashlib.sha1(key + MAGIC).digest()).decode()
        conn.sendall(
            b"HTTP/1.1 101 Switching Protocols\r\n"
            b"Upgrade: websocket\r\n"
            b"Connection: Upgrade\r\n"
            b"Sec-WebSocket-Accept: " + accept.encode() + b"\r\n\r\n"
        )
        conn.sendall(b"\x81\x05hello")
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
python3 - "$FRONT_PORT" <<'PY' || fail=1
import base64
import hashlib
import os
import socket
import struct
import sys

port = int(sys.argv[1])
path = "/ws/tools"
key = base64.b64encode(os.urandom(16)).decode()
req = (
    f"GET {path} HTTP/1.1\r\n"
    f"Host: 127.0.0.1:{port}\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    f"Sec-WebSocket-Key: {key}\r\n"
    f"Sec-WebSocket-Version: 13\r\n"
    "\r\n"
).encode()
sock = socket.create_connection(("127.0.0.1", port), timeout=5)
sock.sendall(req)
resp = b""
while b"\r\n\r\n" not in resp:
    resp += sock.recv(4096)
if b"101" not in resp.split(b"\r\n", 1)[0]:
    raise SystemExit(f"expected 101, got: {resp[:80]!r}")
hdr_len = resp.index(b"\r\n\r\n") + 4
body = resp[hdr_len:]
while len(body) < 2:
    body += sock.recv(4096)
if body[0] & 0x0F != 0x1:
    raise SystemExit("expected text frame")
plen = body[1] & 0x7F
while len(body) < 2 + plen:
    body += sock.recv(4096)
payload = body[2 : 2 + plen].decode()
if payload != "hello":
    raise SystemExit(f"expected hello payload, got {payload!r}")
sock.close()
PY

kill "$FE_PID" "$BE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true

if [[ "$fail" -ne 0 ]]; then
  echo "test-m2-websocket-runtime: FAIL" >&2
  exit 1
fi
echo "test-m2-websocket-runtime: ok (101 upgrade + proxied frame)"
