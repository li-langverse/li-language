#!/usr/bin/env bash
# m15-sse-runtime: live li-httpd SSE relay; idle-between-chunks timeout cancels upstream.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${SSE_IDLE_CFG:-$ROOT/packages/li-net-httpd/examples/sse_idle.toml}"
CONF="/tmp/httpd-sse-idle.conf"
PUBLIC="/tmp/httpd-sse-idle-public"
BE_PORT=18141
FRONT_PORT=18140
STALL_MARK="/tmp/httpd-m15-sse-stall-cancel.ok"

if [[ ! -x "$HTTPD" ]]; then
  echo "test-m15-sse-runtime: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"
rm -f "$STALL_MARK"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q '^listen_port=18140' "$CONF"
grep -q '^stream_idle_timeout_sec=2' "$CONF"

fuser -k "${BE_PORT}/tcp" "${FRONT_PORT}/tcp" 2>/dev/null || true
sleep 0.5

python3 -u - "$BE_PORT" "$STALL_MARK" <<'PY' >/dev/null 2>&1 &
import socket
import sys
import threading
import time

port = int(sys.argv[1])
stall_mark = sys.argv[2]


def read_request(conn: socket.socket) -> bytes:
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            return b""
        data += chunk
    return data


def handle_stall(conn: socket.socket) -> None:
    conn.sendall(
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Type: text/event-stream\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )
    payload = b"data: first\r\n\r\n"
    chunk = f"{len(payload):x}\r\n".encode() + payload + b"\r\n"
    conn.sendall(chunk)
    cancelled = False
    deadline = time.time() + 3.0
    conn.setblocking(False)
    while time.time() < deadline:
        try:
            if conn.recv(4096) == b"":
                cancelled = True
                break
        except BlockingIOError:
            time.sleep(0.05)
        except (ConnectionResetError, BrokenPipeError, OSError):
            cancelled = True
            break
    if not cancelled:
        try:
            payload2 = b"data: late\r\n\r\n"
            chunk2 = f"{len(payload2):x}\r\n".encode() + payload2 + b"\r\n"
            conn.sendall(chunk2)
        except (ConnectionResetError, BrokenPipeError, OSError):
            cancelled = True
    if cancelled:
        try:
            open(stall_mark, "w", encoding="utf-8").write("1")
        except OSError:
            pass
    conn.close()


def handle_stream(conn: socket.socket) -> None:
    conn.sendall(
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Type: text/event-stream\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )
    for i in range(4):
        payload = f"data: tick{i}\r\n\r\n".encode()
        chunk = f"{len(payload):x}\r\n".encode() + payload + b"\r\n"
        conn.sendall(chunk)
        time.sleep(0.05)
    conn.sendall(b"0\r\n\r\n")
    conn.close()


def handle(conn: socket.socket) -> None:
    try:
        req = read_request(conn)
        if not req:
            return
        if b"X-Sse-Stall: 1" in req:
            handle_stall(conn)
        else:
            handle_stream(conn)
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

TP='traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01'

SECONDS=0
stall_code=$(curl -s -N -m 6 -o /tmp/httpd-sse-stall.out -w "%{http_code}" \
  -X POST "http://127.0.0.1:${FRONT_PORT}/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -H "Connection: close" \
  -H "X-Sse-Stall: 1" \
  -H "$TP" \
  -d '{"model":"gpt-4","messages":[]}' 2>/dev/null || true)
stall_code="${stall_code:-000}"
stall_elapsed=$SECONDS
sleep 0.5
stall_cancel=0
if [[ -f "$STALL_MARK" ]]; then
  stall_cancel=1
fi

stream_ok=$(curl -s -N -m 5 -o /tmp/httpd-sse-stream.out -w "%{http_code}" \
  -X POST "http://127.0.0.1:${FRONT_PORT}/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -H "Connection: close" \
  -H "$TP" \
  -d '{"model":"gpt-4","messages":[]}' 2>/dev/null || true)
stream_ok="${stream_ok:-000}"

kill "$FE_PID" "$BE_PID" 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true

fail=0
if [[ "$stream_ok" != "200" ]]; then
  echo "test-m15-sse-runtime: FAIL SSE stream expected 200 got $stream_ok" >&2
  fail=1
fi
if ! grep -q 'data: tick' /tmp/httpd-sse-stream.out 2>/dev/null; then
  echo "test-m15-sse-runtime: FAIL missing relayed SSE events" >&2
  fail=1
fi
if [[ "$stall_code" != "200" && "$stall_code" != "504" ]]; then
  echo "test-m15-sse-runtime: FAIL idle stall expected 200 (truncated) or 504 got $stall_code" >&2
  fail=1
fi
if [[ "$stall_elapsed" -gt 5 ]]; then
  echo "test-m15-sse-runtime: FAIL idle stall took ${stall_elapsed}s (expected ~2s idle cutoff)" >&2
  fail=1
fi
if [[ "$stall_cancel" -ne 1 ]]; then
  echo "test-m15-sse-runtime: FAIL upstream not cancelled after idle timeout" >&2
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "test-m15-sse-runtime: ok (SSE relay, idle-between-chunks cancel upstream)"
