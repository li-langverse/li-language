#!/usr/bin/env bash
# m15-inference-live: OpenAI /v1 routes on live li-httpd — rate limits, OTel traceparent, cancel-on-disconnect.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HTTPD="${LI_HTTPD_BIN:-$ROOT/build/li-httpd}"
CFG="${INFERENCE_LIVE_CFG:-$ROOT/packages/li-net-httpd/examples/inference_live.toml}"
CONF="/tmp/httpd-inference-live.conf"
PUBLIC="$(cd "$ROOT/packages/li-net-httpd/examples" && mkdir -p public && pwd)/public"
BE_PORT=18131
FRONT_PORT=18130
CANCEL_MARK="/tmp/httpd-m15-inference-cancel.ok"

if [[ ! -x "$HTTPD" ]]; then
  echo "test-m15-inference-live: build li-httpd first (./scripts/build-li-httpd.sh)" >&2
  exit 1
fi

mkdir -p "$PUBLIC"
echo ok > "$PUBLIC/index.html"
rm -f "$CANCEL_MARK"

python3 "$ROOT/scripts/validate-httpd-config.py" "$CFG"
python3 "$ROOT/scripts/flatten-httpd-config.py" "$CFG" -o "$CONF"
grep -q '^listen_port=18130' "$CONF"
grep -q '^upstream_peer=18131' "$CONF"
grep -q 'route_require=POST|/v1/chat/completions|traceparent' "$CONF"

fuser -k "${BE_PORT}/tcp" "${FRONT_PORT}/tcp" 2>/dev/null || true
pkill -f 'httpd-inference-live' 2>/dev/null || true
pkill -f '[/]li-httpd.*httpd-inference-live' 2>/dev/null || true
sleep 0.5

python3 -u - "$BE_PORT" "$CANCEL_MARK" <<'PY' >/dev/null 2>&1 &
import socket
import sys
import threading

port = int(sys.argv[1])
cancel_mark = sys.argv[2]
lock = threading.Lock()
seen_traceparent = 0


def read_request(conn: socket.socket) -> bytes:
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            return b""
        data += chunk
    return data


def has_traceparent(req: bytes) -> bool:
    for line in req.split(b"\r\n"):
        if line.lower().startswith(b"traceparent:"):
            return True
    return False


def handle_json(conn: socket.socket, req: bytes) -> None:
    global seen_traceparent
    if has_traceparent(req):
        with lock:
            seen_traceparent += 1
    body = b'{"id":"chatcmpl-test","object":"chat.completion","choices":[]}'
    conn.sendall(
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Type: application/json\r\n"
        b"Content-Length: " + str(len(body)).encode() + b"\r\n"
        b"Connection: close\r\n"
        b"\r\n" + body
    )


def handle_sse(conn: socket.socket, req: bytes) -> None:
    import time

    try:
        open(cancel_mark + ".started", "w", encoding="utf-8").write("1")
    except OSError:
        pass
    conn.sendall(
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Type: text/event-stream\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )
    time.sleep(0.15)
    cancelled = False
    try:
        for _ in range(80):
            payload = b"data: ping\r\n"
            chunk = f"{len(payload):x}\r\n".encode() + payload + b"\r\n"
            conn.sendall(chunk)
            time.sleep(0.08)
    except (ConnectionResetError, BrokenPipeError, OSError):
        cancelled = True
    if cancelled:
        try:
            open(cancel_mark, "w", encoding="utf-8").write("1")
        except OSError:
            pass
    conn.close()


def handle(conn: socket.socket) -> None:
    try:
        req = read_request(conn)
        if not req:
            return
        if b"text/event-stream" in req.lower():
            handle_sse(conn, req)
        else:
            handle_json(conn, req)
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

# Cancel-on-disconnect first (rate bucket still full).
rm -f "$CANCEL_MARK" "${CANCEL_MARK}.started"
cancel_code=$(curl -s -N -m 0.6 -o /dev/null -w "%{http_code}" \
  -X POST "http://127.0.0.1:${FRONT_PORT}/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -H "traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01" \
  -d '{}' || echo "000")
sleep 2.0
cancel_ok=0
if [[ -f "${CANCEL_MARK}.started" && -f "$CANCEL_MARK" ]]; then
  cancel_ok=1
fi

code_no_tp=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -X POST "http://127.0.0.1:${FRONT_PORT}/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -d '{"model":"gpt-4","messages":[]}' || echo "000")

code_ok=$(curl -s -m 3 -o /dev/null -w "%{http_code}" \
  -X POST "http://127.0.0.1:${FRONT_PORT}/v1/chat/completions" \
  -H "Content-Type: application/json" \
  -H "traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01" \
  -d '{"model":"gpt-4","messages":[]}' || echo "000")

got_429=0
for _ in $(seq 1 24); do
  c=$(curl -s -m 2 -o /dev/null -w "%{http_code}" \
    -X POST "http://127.0.0.1:${FRONT_PORT}/v1/chat/completions" \
    -H "Content-Type: application/json" \
    -H "traceparent: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01" \
    -d '{"model":"gpt-4","messages":[]}' || echo "000")
  if [[ "$c" == "429" ]]; then
    got_429=1
    break
  fi
done

kill "$FE_PID" "$BE_PID" 2>/dev/null || true
pkill -f 'httpd-inference-live' 2>/dev/null || true
wait "$FE_PID" 2>/dev/null || true
wait "$BE_PID" 2>/dev/null || true

fail=0
if [[ "$code_no_tp" != "400" ]]; then
  echo "test-m15-inference-live: FAIL missing traceparent expected 400 got $code_no_tp" >&2
  fail=1
fi
if [[ "$code_ok" != "200" ]]; then
  echo "test-m15-inference-live: FAIL with traceparent expected 200 got $code_ok" >&2
  fail=1
fi
if [[ "$got_429" -ne 1 ]]; then
  echo "test-m15-inference-live: FAIL expected 429 from rate limit" >&2
  fail=1
fi
if [[ "$cancel_ok" -ne 1 ]]; then
  echo "test-m15-inference-live: FAIL client disconnect did not cancel upstream SSE (curl=$cancel_code)" >&2
  fail=1
fi

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "test-m15-inference-live: ok (OTel 400/200, rate limit 429, cancel-on-disconnect)"
