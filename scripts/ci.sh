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

li_phase "build"
"$ROOT/scripts/build.sh"

li_phase "stdlib coverage gate"
chmod +x "$ROOT/scripts/check-stdlib-coverage.sh"
"$ROOT/scripts/check-stdlib-coverage.sh"

export LI_REPO_ROOT="$ROOT"
export LIC="$("$ROOT/scripts/resolve-lic.sh")"

li_phase "generate AutoVC (2e)"
"$LIC" build "$ROOT/li-tests/modules/greeter/greeter.li" -o /dev/null

if command -v lake >/dev/null 2>&1; then
  export LI_BUILD_VERIFY_LEAN=1
  li_phase "semantics (2f lake + AutoVC)"
  (cd "$ROOT/docs/semantics" && lake build) || exit 1
fi

li_phase "CVE / security gates"
chmod +x "$ROOT/scripts/ci-security.sh"
"$ROOT/scripts/ci-security.sh"

li_phase "httpd config + routing (M1 prep)"
chmod +x "$ROOT/li-tests/run_httpd_config.sh"
"$ROOT/li-tests/run_httpd_config.sh"

li_phase "E2E li-tests (full manifest)"
"$ROOT/li-tests/run_all.sh"

li_phase "tier 0 physics (strict stability)"
python3 "$ROOT/benchmarks/harness/bench.py" --tier 0

li_phase "race_shared_memory"
"$ROOT/li-tests/run_all.sh" race_shared_memory

li_phase "math_syntax (2h)"
"$ROOT/li-tests/run_all.sh" math_syntax

li_phase "math_linalg (2i)"
"$ROOT/li-tests/run_all.sh" math_linalg

li_phase "workspace build (8a)"
chmod +x "$ROOT/scripts/lic-workspace-build.sh"
"$ROOT/scripts/lic-workspace-build.sh" "$ROOT/packages/li.toml"

li_phase "lip / lit (8b/8e)"
chmod +x "$ROOT/scripts/lip" "$ROOT/scripts/lit" "$ROOT/li-tests/tooling/lip_lit_smoke.sh"
"$ROOT/li-tests/tooling/lip_lit_smoke.sh"

li_phase "encapsulation (2g)"
"$ROOT/li-tests/run_all.sh" encapsulation

li_phase "decorators (7d)"
"$ROOT/li-tests/run_all.sh" decorator_exploits decorators

li_phase "stdlib coverage (8e)"
chmod +x "$ROOT/scripts/check-stdlib-coverage.sh"
"$ROOT/scripts/check-stdlib-coverage.sh"

li_phase "doc provability claims"
chmod +x "$ROOT/scripts/check-doc-provability-claims.sh"
"$ROOT/scripts/check-doc-provability-claims.sh"

li_phase "lic verify smoke (2e/2f)"
chmod +x "$ROOT/scripts/lean-verify-stub.sh" "$ROOT/li-tests/tooling/lic_verify_smoke.sh" \
  "$ROOT/li-tests/tooling/vc_emit_contracts.sh" "$ROOT/li-tests/tooling/contracts_verify_lean.sh"
export LI_REPO_ROOT="$ROOT"
"$ROOT/li-tests/tooling/lic_verify_smoke.sh"
"$ROOT/li-tests/tooling/vc_emit_contracts.sh"
"$ROOT/li-tests/tooling/contracts_verify_lean.sh"

li_phase "lic JSON diagnostics (Vision-LLM)"
chmod +x "$ROOT/li-tests/tooling/diagnose_json_smoke.sh" \
  "$ROOT/li-tests/tooling/error_codes_smoke.sh" "$ROOT/scripts/lic-fix-suggest.sh"
"$ROOT/li-tests/tooling/diagnose_json_smoke.sh"
"$ROOT/li-tests/tooling/error_codes_smoke.sh"

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
