#!/usr/bin/env bash
# Exit gates for lic monorepo v1 (master plan phases 0–7 core, 2g–2h, partial 2e/7d/7e).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"
export LI_REPO_ROOT="$ROOT"
export LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

fail() { li_gate_fail "$*"; exit 1; }
ok() { li_gate_ok "$*"; }

[[ -x "$LIC" ]] || fail "lic not built"

li_banner
li_phase "li-tests core"
"$ROOT/li-tests/run_all.sh" || fail "run_all"
for s in simd race_shared_memory parallel_codegen decorator_exploits decorators \
  stdlib_seal stdlib_coverage math_syntax math_linalg encapsulation modules httpd; do
  "$ROOT/li-tests/run_all.sh" "$s" || fail "suite $s"
done

li_phase "security"
"$ROOT/li-tests/run_security.sh" || fail "security"

li_phase "VC emit on build"
rm -f "$ROOT/build/generated/AutoVC.lean"
"$LIC" build "$ROOT/li-tests/modules/greeter/greeter.li" -o /dev/null
[[ -f "$ROOT/build/generated/AutoVC.lean" ]] || fail "AutoVC.lean not emitted"

li_phase "proof-db release gate (advisory)"
chmod +x "$ROOT/scripts/check-proof-db.sh"
export LI_PROOF_DB_STRICT="${LI_PROOF_DB_STRICT:-0}"
"$ROOT/scripts/check-proof-db.sh" || li_warn "proof-db drift — proof-db/baseline.jsonl"

if command -v lake >/dev/null 2>&1; then
  li_phase "semantics lake"
  (cd "$ROOT/docs/semantics" && lake build) || fail "lake build"
  "$ROOT/scripts/check-autovc-open-goals.sh" "$ROOT/build/generated/AutoVC.lean" || fail "autovc open goals"
else
  li_warn "semantics lake N/A — lake not on PATH"
fi

li_phase "tier 0 bench"
python3 "$ROOT/benchmarks/harness/bench.py" --tier 0 || fail "bench tier 0"

li_phase "toolchain / doc / package smoke"
chmod +x "$ROOT/scripts/check-doc-provability-claims.sh" \
  "$ROOT/scripts/check-li-toolchain.sh" \
  "$ROOT/li-tests/tooling/li_new_package_smoke.sh" \
  "$ROOT/li-tests/tooling/lip_lit_smoke.sh" \
  "$ROOT/li-tests/tooling/lip_publish_smoke.sh" \
  "$ROOT/li-tests/tooling/vc_emit_contracts.sh" \
  "$ROOT/li-tests/tooling/discharge_trivial_lean.sh" \
  "$ROOT/li-tests/tooling/discharge_const_lean.sh" \
  "$ROOT/li-tests/tooling/discharge_sqrt_contract_lean.sh" \
  "$ROOT/li-tests/tooling/check_release_bounds_ir.sh" \
  "$ROOT/li-tests/tooling/contracts_discharge_corpus.sh" \
  "$ROOT/li-tests/tooling/contracts_verify_lean.sh" \
  "$ROOT/li-tests/tooling/mir_vc_witness.sh" \
  "$ROOT/li-tests/tooling/diagnose_json_smoke.sh"
"$ROOT/scripts/check-doc-provability-claims.sh"
"$ROOT/scripts/check-mir-parallel-decorator.sh"
"$ROOT/scripts/check-mir-vectorized-decorator.sh"
"$ROOT/scripts/check-li-toolchain.sh"
"$ROOT/li-tests/tooling/li_new_package_smoke.sh"
"$ROOT/li-tests/tooling/lip_lit_smoke.sh"
"$ROOT/li-tests/tooling/vc_emit_contracts.sh"
"$ROOT/li-tests/tooling/discharge_trivial_lean.sh"
"$ROOT/li-tests/tooling/discharge_sqrt_contract_lean.sh"
"$ROOT/li-tests/tooling/contracts_discharge_corpus.sh"
"$ROOT/li-tests/tooling/diagnose_json_smoke.sh"

li_phase "hpc competitive registry (optional advisory)"
chmod +x "$ROOT/li-tests/tooling/hpc_competitive_registry.sh"
export LI_HPC_COMPETITIVE_STRICT=0
"$ROOT/li-tests/tooling/hpc_competitive_registry.sh" || li_warn "hpc competitive registry — see benchmarks/competitive/registry.toml"

if command -v lake >/dev/null 2>&1; then
  li_phase "G-lean AutoVC lake typecheck (LiArray)"
  chmod +x "$ROOT/li-tests/tooling/autovc_lake_typecheck.sh"
  "$ROOT/li-tests/tooling/autovc_lake_typecheck.sh" || fail "autovc lake typecheck"
  li_phase "G-lean strict build smoke (--strict-lean)"
  chmod +x "$ROOT/li-tests/tooling/glean_strict_build_smoke.sh"
  "$ROOT/li-tests/tooling/glean_strict_build_smoke.sh" || fail "glean strict smoke"
fi

li_phase "tier-1 Li vs C++ (advisory; strict via LI_TIER1_PERF_STRICT=1)"
chmod +x "$ROOT/scripts/check-tier1-li-vs-cpp.sh" "$ROOT/li-tests/tooling/tier1_li_vs_cpp.sh"
export LI_TIER1_PERF_STRICT="${LI_TIER1_PERF_STRICT:-0}"
"$ROOT/li-tests/tooling/tier1_li_vs_cpp.sh" || li_warn "tier-1 perf gaps — see benchmarks/results/latest.csv"


ok "lic monorepo v1 gates"
