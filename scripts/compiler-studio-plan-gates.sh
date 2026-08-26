#!/usr/bin/env bash
# Gates for compiler+Studio plan loop (Wave A first). Not httpd.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=llvm-env.sh
source "$ROOT/scripts/llvm-env.sh"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"
li_detect_compilers
export LI_REPO_ROOT="$ROOT"
export CC CXX
export LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

fail() { li_gate_fail "$*"; exit 1; }

if [[ "${COMPILER_STUDIO_GATES_SKIP_LIC_BUILD:-0}" == "1" ]]; then
  li_warn "skip lic build (COMPILER_STUDIO_GATES_SKIP_LIC_BUILD=1)"
else
  [[ -x "$LIC" ]] || fail "lic not built — run ./scripts/build.sh"
  li_phase "math_linalg suite"
  "$ROOT/li-tests/run_all.sh" math_linalg || fail "math_linalg"
  li_phase "math/physics goldens"
  "$ROOT/scripts/verify-math-physics-goldens.sh" || fail "verify-math-physics-goldens"
  if command -v clang-22 >/dev/null 2>&1 || command -v clang >/dev/null 2>&1; then
    li_phase "tier-1 result verify"
    python3 "$ROOT/benchmarks/harness/bench.py" --verify-results --tier 1 || fail "bench verify tier 1"
  else
    li_warn "skip tier-1 verify (no clang)"
  fi
  if command -v lake >/dev/null 2>&1; then
    li_phase "Lean AutoVC smoke"
    chmod +x "$ROOT/li-tests/tooling/autovc_lake_typecheck.sh" \
      "$ROOT/li-tests/tooling/glean_strict_build_smoke.sh"
    "$ROOT/li-tests/tooling/autovc_lake_typecheck.sh" || fail "autovc lake"
    "$ROOT/li-tests/tooling/glean_strict_build_smoke.sh" || fail "glean strict"
  fi
fi

li_phase "tier 0 bench"
python3 "$ROOT/benchmarks/harness/bench.py" --tier 0 || fail "bench tier 0"

if [[ "${COMPILER_STUDIO_GATES_FULL:-0}" == "1" ]]; then
  li_phase "full master-plan gates"
  "$ROOT/scripts/check-master-plan-gates.sh" || fail "check-master-plan-gates"
fi

li_ok "compiler-studio plan gates"
