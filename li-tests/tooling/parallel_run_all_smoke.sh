#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC_BIN="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
if [[ ! -x "$LIC_BIN" ]]; then echo "parallel_run_all_smoke: skip (no lic)" >&2; exit 0; fi
fail() { echo "parallel_run_all_smoke: $*" >&2; exit 1; }
NULL_OUT="/dev/null"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) NULL_OUT="NUL" ;; esac
run_job() {
  local id="$1"
  local file="$2"
  local build_dir="$ROOT/build/li-test-parallel-smoke-${id}"
  mkdir -p "$build_dir/generated"
  "$LIC_BIN" build --build-dir="$build_dir" "$ROOT/li-tests/$file" -o "$NULL_OUT" >/dev/null 2>&1 || return 1
  [[ -f "$build_dir/generated/AutoVC.lean" ]] || return 1
}
run_job 0 "typecheck/fib.li" & p0=$!
run_job 1 "math_linalg/broadcast_len1_add_float4.li" & p1=$!
wait "$p0" || fail "worker 0"
wait "$p1" || fail "worker 1"
echo "parallel_run_all_smoke: ok"
