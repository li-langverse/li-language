#!/usr/bin/env bash
# 2f: http extern-forward wrapper must have zero open Prop goals (w0-lean-gate slice).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export LI_REPO_ROOT="$ROOT"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
li_detect_compilers
export CC CXX
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
SAMPLE="$ROOT/li-tests/contracts_verify/http_parse_forward_closed.li"
AUTOVC="$ROOT/build/generated/AutoVC.lean"
rm -f "$AUTOVC"
"$LIC" build "$SAMPLE" -o /dev/null
test -f "$AUTOVC"
chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
"$ROOT/scripts/check-autovc-open-goals.sh" "$AUTOVC"
if command -v lake >/dev/null 2>&1; then
  (cd "$ROOT/docs/semantics" && lake build AutoVC Discharge) || exit $?
  echo "discharge_http_forward_lean: lake ok"
fi
echo "discharge_http_forward_lean: ok"
