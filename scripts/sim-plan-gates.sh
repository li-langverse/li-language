#!/usr/bin/env bash
# Sim plan gates: validity + performance + memory (package-scoped).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
export LIC="$("$ROOT/scripts/resolve-lic.sh")"

fail() { echo "sim-plan-gates: $*" >&2; exit 1; }

PKG="${SIM_PLAN_PACKAGE:-li-sim-scientific}"
RUNS="${SIM_PLAN_BENCH_RUNS:-1}"
SCOPE_JSON="$(python3 "$ROOT/benchmarks/harness/bench_scope.py" --package "$PKG" --json)"
BENCHES="$(python3 -c "import json,sys; print(','.join(json.load(sys.stdin)['benches']))" <<<"$SCOPE_JSON")"

echo "==> sim-plan-gates package=$PKG benches=[$BENCHES]"

if [[ ! -x "$LIC" ]]; then
  echo "==> build lic"
  "$ROOT/scripts/build.sh" >/dev/null
fi

echo "==> validity (composable + summaries + registry)"
python3 "$ROOT/benchmarks/harness/bench_sim.py" --package "$PKG" --write-summary \
  || fail "bench_sim validity"

./scripts/validate-sim-summary.sh || fail "validate-sim-summary"

if [[ -n "$BENCHES" ]]; then
  echo "==> validity (numerical verify-results, scoped)"
  python3 "$ROOT/benchmarks/harness/bench.py" --verify-results --tier 2 \
    --package "$PKG" || fail "verify-results tier-2"
fi

echo "==> performance (scoped timing → latest.csv)"
if [[ -n "$BENCHES" ]]; then
  "$ROOT/scripts/bench-package.sh" "$PKG" --timing --runs "$RUNS" --write-summary \
    || fail "bench-package timing"
fi

echo "==> memory (peak RSS native binaries)"
export SIM_PLAN_PACKAGE="$PKG"
"$ROOT/scripts/sim-bench-memory.sh" "$BENCHES" || fail "sim-bench-memory"

echo "==> iteration report"
python3 "$ROOT/scripts/sim-plan-iteration-report.py" --package "$PKG" || fail "iteration report"

echo "sim-plan-gates: ok"
