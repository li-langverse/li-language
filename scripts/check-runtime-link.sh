#!/usr/bin/env bash
# Smoke: microbenches must not link the full httpd/net runtime unless used.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"

LIC="${LIC:-$ROOT/build/compiler/lic/lic}"
if [[ ! -x "$LIC" ]]; then
  li_fail "missing lic — run ./scripts/build.sh"
  exit 1
fi

export CC="${CC:-clang-22}"
export CXX="${CXX:-clang++-22}"

HORNER_BIN="$ROOT/build/bench/runtime_link_smoke_horner"
HTTPD_SRC="$ROOT/li-tests/httpd/minimal_route_match.li"
if [[ ! -f "$HTTPD_SRC" ]]; then
  HTTPD_SRC="$ROOT/benchmarks/tier1_micro/horner_pure_li/li/main.li"
fi

li_phase "runtime link: horner (no epoll)"
"$LIC" build "$ROOT/benchmarks/tier1_micro/horner_pure_li/li/main.li" -o "$HORNER_BIN" \
  --allow-open-vc --no-lean-verify --release -O3 -march=native -ffast-math
if nm "$HORNER_BIN" 2>/dev/null | grep -q 'epoll_create1_i'; then
  li_fail "horner binary must not link li_rt_net (found epoll_create1_i)"
  exit 1
fi
HORNER_BYTES=$(wc -c <"$HORNER_BIN")
if [[ "$HORNER_BYTES" -gt 80000 ]]; then
  li_fail "horner binary too large (${HORNER_BYTES} bytes) — likely linked full net runtime"
  exit 1
fi
li_ok "horner slim link (${HORNER_BYTES} bytes, no epoll)"

li_ok "runtime link check finished"
