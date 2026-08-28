#!/usr/bin/env bash
# Runtime gate: httpd epoll seam must link and run on every platform.
#
# Regression for the missing macOS/#else stub: `epoll_wait_tagged_timeout_ms_i`
# is declared in std/runtime/seam.li and defined in runtime/li_rt_net.c only
# under `#ifdef __linux__`, with no `#else` stub — so li-net-httpd failed at
# link time on macOS/arm64 with "symbol(s) not found". This gate builds + runs
# a program that calls the seam and asserts the non-Linux stub answers -1
# (the Linux implementation can only return >= 0 or block, never -1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
TMP="$(mktemp -d)"
BIN="$TMP/httpd_epoll_seam"
SRC="$TMP/httpd_epoll_seam.li"

cat > "$SRC" <<'EOF'
# Runtime regression: the httpd epoll seam must link on non-Linux (missing #else stub).
extern def epoll_wait_tagged_timeout_ms_i(epfd: var int, events: var ptr, max_events: var int, timeout_ms: var int) -> int
  requires epfd >= 0
  ensures true
  decreases 0

def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var epfd: int = 0
  var events: ptr
  var max_events: int = 0
  var timeout_ms: int = 0
  var n: int = epoll_wait_tagged_timeout_ms_i(epfd, events, max_events, timeout_ms)
  # Non-Linux stub returns -1; if the symbol is missing the link fails outright.
  if n != -1:
    return 1
  return 0
EOF

"$LIC" build "$SRC" -o "$BIN" --allow-open-vc --no-lean-verify >/dev/null 2>&1
"$BIN"
rc=$?
rm -rf "$TMP"
if [[ $rc -ne 0 ]]; then
  echo "check_httpd_epoll_seam_runtime: FAILED (rc=$rc)" >&2
  exit 1
fi
