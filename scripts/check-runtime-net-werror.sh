#!/usr/bin/env bash
# Gate: runtime/li_rt_net.c must compile warning-free under -Werror.
#
# Catches platform-stub hygiene regressions at compile time: the macOS
# #else branch of the Net seam (epoll/poll stubs, proxy glue) must keep
# compiling clean with -Wall -Wextra. A missing stub like
# `epoll_wait_tagged_timeout_ms_i` historically failed only at *link* time
# on macOS; this phase catches the conditional-compilation mistakes around
# those branches early, in CI, on every platform.
#
# Usage:
#   scripts/check-runtime-net-werror.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-clang}"

"$CC" -Werror -Wall -Wextra -fsyntax-only -I"$ROOT/runtime" "$ROOT/runtime/li_rt_net.c"

echo "check-runtime-net-werror: ok (li_rt_net.c compiles clean under -Werror)"
