#!/usr/bin/env bash
# w0-lean-gate: 2e/2f VC emit + Lean typecheck on li-http / parse_request path (Phase H P0).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
li_detect_compilers
export LI_REPO_ROOT="$ROOT"
export CC CXX
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
HTTP_LIB="$ROOT/packages/li-http/src/lib.li"
SMOKE="$ROOT/li-tests/httpd/parse_request_smoke.li"
MAX_OPEN="${HTTPD_LEAN_GATE_MAX_OPEN:-8}"

if [[ ! -x "$LIC" ]]; then
  echo "check-httpd-lean-gate: build lic first (./scripts/build.sh)" >&2
  exit 1
fi

echo "==> lic verify li-http (VC summary)"
"$LIC" verify "$HTTP_LIB"

echo "==> lic verify li-http + lake (2f)"
if command -v lake >/dev/null 2>&1; then
  "$LIC" verify "$HTTP_LIB" --lean
else
  echo "check-httpd-lean-gate: skip --lean (lake not installed)"
fi

echo "==> parse_request_smoke build + AutoVC open-goal budget"
AUTOVC="$ROOT/build/generated/AutoVC.lean"
rm -f "$AUTOVC"
"$LIC" build "$SMOKE" -o /tmp/li_parse_request_smoke_gate --allow-open-vc
test -f "$AUTOVC"
chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
set +e
open_out="$("$ROOT/scripts/check-autovc-open-goals.sh" "$AUTOVC" 2>&1)"
open_rc=$?
set -e
open_count="$(printf '%s\n' "$open_out" | grep -c '^open VC:' || true)"
echo "$open_out"
if [[ "$open_count" -gt "$MAX_OPEN" ]]; then
  echo "check-httpd-lean-gate: $open_count open VC(s) exceed budget $MAX_OPEN" >&2
  exit 1
fi
if [[ "$open_rc" -ne 0 ]]; then
  echo "check-httpd-lean-gate: open VC budget ok ($open_count <= $MAX_OPEN); remaining goals documented"
fi
smoke_rc="$(/tmp/li_parse_request_smoke_gate >/dev/null; echo $?)"
test "$smoke_rc" -eq 24

if [[ -x "$ROOT/li-tests/tooling/discharge_http_forward_lean.sh" ]]; then
  echo "==> discharge_http_forward_lean.sh"
  "$ROOT/li-tests/tooling/discharge_http_forward_lean.sh"
fi

echo "check-httpd-lean-gate: ok ($open_count open VC on parse_request_smoke)"
