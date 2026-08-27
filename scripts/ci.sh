#!/usr/bin/env bash
# CI entry: build lic, li-tests, tier-0 benchmarks (verify + stability).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"
export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"

li_banner
li_phase "agent-kit sync check"
chmod +x "$ROOT/scripts/check-agent-kit-sync.sh" 2>/dev/null || true
if [[ -x "$ROOT/scripts/check-agent-kit-sync.sh" ]]; then
  "$ROOT/scripts/check-agent-kit-sync.sh" || {
    echo "hint: run ../roadmap/scripts/install-agent-kit.sh from sibling roadmap checkout" >&2
    exit 1
  }
fi

li_phase "def syntax policy"
chmod +x "$ROOT/scripts/check-li-def-syntax.sh"
"$ROOT/scripts/check-li-def-syntax.sh" "$ROOT"

li_phase "build"
"$ROOT/scripts/build.sh"

li_phase "runtime net -Werror hygiene"
chmod +x "$ROOT/scripts/check-runtime-net-werror.sh"
"$ROOT/scripts/check-runtime-net-werror.sh"

li_phase "compiler-only golden lowering"
chmod +x "$ROOT/scripts/check-compiler-goldens.sh"
"$ROOT/scripts/check-compiler-goldens.sh"

li_phase "self-hosted front end (lexer + parser + AST + self-front-end + check + MIR parity)"
chmod +x "$ROOT/scripts/check_li_parity.sh"
"$ROOT/scripts/check_li_parity.sh"

li_phase "stage-2 self-host frontend"
chmod +x "$ROOT/scripts/check_li_stage2_frontend.sh"
"$ROOT/scripts/check_li_stage2_frontend.sh"

li_phase "compiler isolation (carve-out)"
chmod +x "$ROOT/scripts/check-compiler-isolation.sh"
"$ROOT/scripts/check-compiler-isolation.sh"

li_phase "per-package standalone install (carve-out)"
chmod +x "$ROOT/scripts/check-package-standalone.sh"
# Full workspace sweep is heavy (~minutes); the nightly job (LI_NIGHTLY=1)
# runs every workspace member, while regular CI runs the core + newly-fixed
# packages for fast feedback.
if [[ "${LI_NIGHTLY:-0}" == "1" ]]; then
  echo "nightly mode: full ${LI_NIGHTLY_PKGS:-all-members} standalone sweep"
  "$ROOT/scripts/check-package-standalone.sh"
else
  "$ROOT/scripts/check-package-standalone.sh" li-aimd li-chem li-sim li-nanoreactor li-math \
    li-math-numerics li-core li-physics-core li-physics-rigid li-physics-runtime \
    li-physics-em li-physics-fluids li-physics-weather li-net-httpd li-render li-studio \
    li-lig-present lig li-scene li-gui
fi

li_phase "stdlib coverage gate"
chmod +x "$ROOT/scripts/check-stdlib-coverage.sh"
"$ROOT/scripts/check-stdlib-coverage.sh"

export LI_REPO_ROOT="$ROOT"
export LIC="$("$ROOT/scripts/resolve-lic.sh")"
export CI=true
# Wave 2 integrator (8p-d): explicit parallel flags — do not export LI_TEST_JOBS.
RUN_ALL_FLAGS=(-j8 --max-memory=8192)

li_phase "generate AutoVC (2e)"
"$LIC" build "$ROOT/li-tests/modules/greeter/greeter.li" -o /dev/null

if command -v lake >/dev/null 2>&1; then
  li_phase "semantics (2f lake + AutoVC strict)"
  (cd "$ROOT/docs/semantics" && lake build) || exit 1
  "$ROOT/scripts/check-autovc-open-goals.sh" "$ROOT/build/generated/AutoVC.lean" || exit 1
fi

li_phase "CVE / security gates"
chmod +x "$ROOT/scripts/ci-security.sh"
"$ROOT/scripts/ci-security.sh"

li_phase "httpd config + routing (M1 prep)"
chmod +x "$ROOT/li-tests/run_httpd_config.sh" \
  "$ROOT/scripts/li-httpd-explain-config.sh" \
  "$ROOT/scripts/check-httpd-explain-config.sh" \
  "$ROOT/scripts/check-httpd-config-desugar.sh" \
  "$ROOT/scripts/lic-validate-httpd-config.sh" \
  "$ROOT/scripts/flatten-httpd-config.py" \
  "$ROOT/scripts/validate-httpd-config.py"
"$ROOT/li-tests/run_httpd_config.sh"
if [[ "${HTTPD_SKIP_AUTH_BEARER_SMOKE:-0}" == "1" ]]; then
  echo "skip test-auth-bearer (HTTPD_SKIP_AUTH_BEARER_SMOKE=1)"
elif [[ "$(uname -s)" == "Darwin" && "${HTTPD_SKIP_AUTH_BEARER_DARWIN:-1}" != "0" ]]; then
  echo "skip test-auth-bearer (Darwin CI: bearer TCP smoke runs on Linux)"
elif [[ -x "$ROOT/scripts/build-li-httpd.sh" ]] \
  && "$ROOT/scripts/build-li-httpd.sh"; then
  chmod +x "$ROOT/scripts/test-auth-bearer.sh"
  "$ROOT/scripts/test-auth-bearer.sh"
fi

li_phase "E2E li-tests (full manifest)"
"$ROOT/li-tests/run_all.sh" "${RUN_ALL_FLAGS[@]}"

li_phase "tier 0 physics (strict stability)"
python3 "$ROOT/benchmarks/harness/bench.py" --tier 0

li_phase "race_shared_memory"
"$ROOT/li-tests/run_all.sh" "${RUN_ALL_FLAGS[@]}" race_shared_memory

li_phase "math_syntax (2h)"
"$ROOT/li-tests/run_all.sh" "${RUN_ALL_FLAGS[@]}" math_syntax

li_phase "math_linalg (2i)"
"$ROOT/li-tests/run_all.sh" "${RUN_ALL_FLAGS[@]}" math_linalg

li_phase "workspace build (8a)"
chmod +x "$ROOT/scripts/lic-workspace-build.sh"
"$ROOT/scripts/lic-workspace-build.sh" "$ROOT/packages/li.toml"

# WP3 (lic check --workspace --jobs=8): skipped until workspace check merges on top of #205.

li_phase "lip / lit (8b/8e)"
chmod +x "$ROOT/scripts/lip" "$ROOT/scripts/lit" "$ROOT/li-tests/tooling/lip_lit_smoke.sh"
"$ROOT/li-tests/tooling/lip_lit_smoke.sh"

li_phase "encapsulation (2g)"
"$ROOT/li-tests/run_all.sh" "${RUN_ALL_FLAGS[@]}" encapsulation

li_phase "decorators (7d)"
"$ROOT/li-tests/run_all.sh" "${RUN_ALL_FLAGS[@]}" decorator_exploits decorators

li_phase "stdlib coverage (8e)"
chmod +x "$ROOT/scripts/check-stdlib-coverage.sh"
"$ROOT/scripts/check-stdlib-coverage.sh"

li_phase "doc provability claims"
chmod +x "$ROOT/scripts/check-doc-provability-claims.sh"
"$ROOT/scripts/check-doc-provability-claims.sh"

li_phase "lic verify smoke (2e/2f)"
chmod +x "$ROOT/scripts/lean-verify-stub.sh" "$ROOT/li-tests/tooling/lic_verify_smoke.sh" \
  "$ROOT/li-tests/tooling/vc_emit_contracts.sh" "$ROOT/li-tests/tooling/contracts_verify_lean.sh" \
  "$ROOT/li-tests/tooling/check_float_array_codegen_runtime.sh" \
  "$ROOT/li-tests/tooling/check_httpd_epoll_seam_runtime.sh"
export LI_REPO_ROOT="$ROOT"
"$ROOT/li-tests/tooling/lic_verify_smoke.sh"
"$ROOT/li-tests/tooling/vc_emit_contracts.sh"
"$ROOT/li-tests/tooling/contracts_verify_lean.sh"
"$ROOT/li-tests/tooling/check_float_array_codegen_runtime.sh"
"$ROOT/li-tests/tooling/check_httpd_epoll_seam_runtime.sh"

li_phase "lic JSON diagnostics (Vision-LLM)"
chmod +x "$ROOT/li-tests/tooling/diagnose_json_smoke.sh" \
  "$ROOT/li-tests/tooling/check_workspace_cache_smoke.sh" \
  "$ROOT/li-tests/tooling/run_all_parallel_smoke.sh" \
  "$ROOT/li-tests/tooling/agent_manifest_smoke.sh" \
  "$ROOT/scripts/export-li-tests-agent-slice.sh" \
  "$ROOT/scripts/lic-fix-suggest.sh"
"$ROOT/li-tests/tooling/diagnose_json_smoke.sh"
"$ROOT/li-tests/tooling/check_workspace_cache_smoke.sh"
"$ROOT/li-tests/tooling/run_all_parallel_smoke.sh"
"$ROOT/li-tests/tooling/agent_manifest_smoke.sh"

li_phase "8p parallel smokes"
chmod +x "$ROOT/li-tests/tooling/ci_test_jobs_smoke.sh"   "$ROOT/li-tests/tooling/resource_flags_smoke.sh"   "$ROOT/li-tests/tooling/parallel_run_all_smoke.sh"
"$ROOT/li-tests/tooling/ci_test_jobs_smoke.sh"
"$ROOT/li-tests/tooling/resource_flags_smoke.sh"
"$ROOT/li-tests/tooling/parallel_run_all_smoke.sh"

li_phase "8-sync toolchain"
chmod +x "$ROOT/scripts/check-li-toolchain.sh"
"$ROOT/scripts/check-li-toolchain.sh"

li_phase "package scaffold smoke"
chmod +x "$ROOT/li-tests/tooling/li_new_package_smoke.sh"
"$ROOT/li-tests/tooling/li_new_package_smoke.sh"

li_phase "traceability (official packages)"
chmod +x "$ROOT/scripts/check-traceability.sh"
"$ROOT/scripts/check-traceability.sh"

li_phase "master plan v1 gates"
chmod +x "$ROOT/scripts/check-master-plan-gates.sh"
"$ROOT/scripts/check-master-plan-gates.sh"

li_gate_ok "continuous integration"
