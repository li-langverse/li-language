#!/usr/bin/env bash
# Golden: C li_rt_httpd_explain_config matches Python httpd_config.py --explain.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CFG="${1:-$ROOT/li-tests/config_desugar/good/agent_gateway.toml}"
GOLDEN="${2:-$ROOT/li-tests/config_desugar/good/agent_gateway.explained.golden}"
CC="${CC:-clang}"
BIN="${TMPDIR:-/tmp}/li_httpd_explain_$$"
trap 'rm -f "$BIN"' EXIT

"$ROOT/scripts/li-httpd-explain-config.sh" "$CFG" >"${BIN}.py"
# li_rt_net.c calls httpd_tls_* / httpd_h2_* — link tls+h2 with net (mirrors compile.cpp).
"$CC" -Wno-override-module -x c "$ROOT/runtime/li_rt.c" -x c "$ROOT/runtime/li_rt_net.c" \
  -x c "$ROOT/runtime/li_rt_log.c" -x c "$ROOT/runtime/li_rt_httpd.c" \
  -x c "$ROOT/runtime/li_rt_tls.c" -x c "$ROOT/runtime/li_rt_h2.c" \
  -x c "$ROOT/scripts/httpd_explain_main.c" \
  -I"$ROOT/runtime" -lm -ldl -o "$BIN"
"$BIN" "$CFG" >"${BIN}.c"
if ! diff -u "$GOLDEN" "${BIN}.c" >/dev/null; then
  echo "check-httpd-explain-config: C vs golden mismatch" >&2
  diff -u "$GOLDEN" "${BIN}.c" >&2 || true
  exit 1
fi
if ! diff -u "${BIN}.py" "${BIN}.c" >/dev/null; then
  echo "check-httpd-explain-config: Python vs C mismatch" >&2
  diff -u "${BIN}.py" "${BIN}.c" >&2 || true
  exit 1
fi
echo "check-httpd-explain-config: ok"
