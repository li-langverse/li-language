#!/usr/bin/env bash
# h-lean-server-modules: Lean typecheck on li-net-httpd (ship lib + main) without --no-lean-verify.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
li_detect_compilers
export LI_REPO_ROOT="$ROOT"
export CC CXX
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
HTTPD_LIB="$ROOT/packages/li-net-httpd/src/lib.li"
HTTPD_MAIN="$ROOT/packages/li-net-httpd/src/main.li"
MAX_OPEN="${HTTPD_SERVER_LEAN_MAX_OPEN:-8}"

if [[ ! -x "$LIC" ]]; then
  echo "check-httpd-server-lean-gate: build lic first (./scripts/build.sh)" >&2
  exit 1
fi

if ! command -v lake >/dev/null 2>&1; then
  echo "check-httpd-server-lean-gate: skip (lake not installed)"
  exit 0
fi

lean_build() {
  local src="$1"
  local label="$2"
  echo "==> lic build $label (Lean on, --allow-open-vc)"
  rm -f "$ROOT/build/generated/AutoVC.lean"
  "$LIC" build "$src" -o "/tmp/li_httpd_server_lean_${label}" --allow-open-vc
  test -f "$ROOT/build/generated/AutoVC.lean"
  chmod +x "$ROOT/scripts/check-autovc-open-goals.sh"
  set +e
  open_out="$("$ROOT/scripts/check-autovc-open-goals.sh" "$ROOT/build/generated/AutoVC.lean" 2>&1)"
  open_rc=$?
  set -e
  open_count="$(printf '%s\n' "$open_out" | grep -c '^open VC:' || true)"
  echo "$open_out"
  if [[ "$open_count" -gt "$MAX_OPEN" ]]; then
    echo "check-httpd-server-lean-gate: $label has $open_count open VC(s) > budget $MAX_OPEN" >&2
    exit 1
  fi
  if [[ "$open_rc" -ne 0 ]]; then
    echo "check-httpd-server-lean-gate: $label open VC budget ok ($open_count <= $MAX_OPEN)"
  fi
}

lean_build "$HTTPD_LIB" "lib"
lean_build "$HTTPD_MAIN" "main"

echo "check-httpd-server-lean-gate: ok (li-net-httpd lib + main, lake typecheck)"
