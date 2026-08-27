#!/usr/bin/env bash
# w1-async-reactor: epoll/kqueue behind li_async_poll + TCP echo benchmark scenario.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
li_detect_compilers
export LI_REPO_ROOT="$ROOT"
export CC CXX
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

if [[ ! -x "$LIC" ]]; then
  echo "check-w1-async-reactor: build lic first (./scripts/build.sh)" >&2
  exit 1
fi

NET_C="$ROOT/runtime/li_rt_net.c"
echo "==> reactor uses epoll/kqueue in li_rt_net.c"
grep -q 'li_async_poll' "$NET_C"
grep -qE 'epoll_wait|kevent' "$NET_C"
if grep -q 'li_async_poll' "$ROOT/runtime/li_rt.c" 2>/dev/null; then
  echo "check-w1-async-reactor: li_async_poll must live in li_rt_net.c only" >&2
  exit 1
fi

echo "==> async/await codegen"
"$LIC" build "$ROOT/li-tests/async/await_codegen_ok.li" \
  -o /tmp/li_await_codegen_gate --allow-open-vc --no-lean-verify
/tmp/li_await_codegen_gate
test "$(/tmp/li_await_codegen_gate; echo $?)" -eq 0

echo "==> reactor selftest link"
"$LIC" build "$ROOT/li-tests/async/reactor_selftest_link.li" \
  -o /tmp/li_async_reactor_gate --allow-open-vc --no-lean-verify
if [[ "$(uname -s)" == "Linux" || "$(uname -s)" == "Darwin" ]]; then
  /tmp/li_async_reactor_gate
  test "$(/tmp/li_async_reactor_gate; echo $?)" -eq 0
else
  echo "==> skip reactor runtime (not Linux/Darwin)"
fi

echo "==> tier5 tcp_echo scenario TOML"
python3 - <<'PY' "$ROOT"
import sys
from pathlib import Path
root = Path(sys.argv[1])
bench = root / "benchmarks/tier5_http/scenarios/tcp_echo/bench.toml"
assert bench.is_file(), bench
text = bench.read_text()
assert "tcp_echo" in text
assert "tcpkali" in text
print("tcp_echo bench.toml ok")
PY

if [[ "$(uname -s)" == "Linux" ]]; then
  echo "==> TCP echo epoll smoke (loopback)"
  PORT="${LI_TCP_ECHO_PORT:-19876}"
  "$LIC" build "$ROOT/li-tests/async/tcp_echo_smoke.li" \
    -o /tmp/li_tcp_echo_smoke --allow-open-vc --no-lean-verify
  /tmp/li_tcp_echo_smoke &
  pid=$!
  sleep 0.2
  payload="ping-w1"
  got="$(python3 -c "import socket;s=socket.create_connection(('127.0.0.1',int('${PORT}')),timeout=3);s.sendall(b'${payload}');print(s.recv(64).decode(),end='')")"
  wait "$pid" 2>/dev/null || true
  test "$got" = "$payload"
fi

echo "==> li-net package smoke"
"$LIC" check "$ROOT/packages/li-net/src/lib.li"

echo "check-w1-async-reactor: OK"
