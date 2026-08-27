#!/usr/bin/env bash
# Build packages/li-net-httpd → build/li-httpd (CLI main + Li epoll loop).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
li_detect_compilers
export LI_REPO_ROOT="$ROOT"
export CC CXX
LIC="$("$ROOT/scripts/resolve-lic.sh")"
mkdir -p "$ROOT/build"
export LI_LINK_RUNTIME_FULL=1
LEAN_FLAGS=()
if ! command -v lake >/dev/null 2>&1; then
  LEAN_FLAGS+=(--no-lean-verify)
fi
"$LIC" build --allow-open-vc "${LEAN_FLAGS[@]}" \
  "$ROOT/packages/li-net-httpd/src/main.li" -o "$ROOT/build/li-httpd"
if [[ -f "$ROOT/li-tests/httpd/m15_leak_censor_oracle.li" ]]; then
  "$LIC" build --allow-open-vc "${LEAN_FLAGS[@]}" \
    "$ROOT/li-tests/httpd/m15_leak_censor_oracle.li" -o "$ROOT/build/li_m15_leak_censor_oracle"
fi
echo "build-li-httpd: ok → $ROOT/build/li-httpd"
