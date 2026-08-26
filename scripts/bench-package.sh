#!/usr/bin/env bash
# Run only benchmarks/tests tied to workspace package(s) — no full tier-12 sweep.
# Usage:
#   ./scripts/bench-package.sh li-sim-scientific
#   ./scripts/bench-package.sh --changed
#   ./scripts/bench-package.sh li-physics-particles --timing --runs 3
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"
export LIC="${LIC:-$($ROOT/scripts/resolve-lic.sh)}"

CHANGED=0
TIMING=0
VERIFY=1
RUNS=1
PACKAGES=()
WRITE_SUMMARY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --changed) CHANGED=1; shift ;;
    --timing) TIMING=1; shift ;;
    --runs) RUNS="${2:?}"; shift 2 ;;
    --skip-verify) VERIFY=0; shift ;;
    --write-summary) WRITE_SUMMARY=1; shift ;;
    -h|--help)
      sed -n '2,8p' "$0"
      exit 0
      ;;
    --) shift; break ;;
    -*) echo "unknown flag: $1" >&2; exit 2 ;;
    *) PACKAGES+=("$1"); shift ;;
  esac
done

SCOPE_ARGS=()
if [[ "$CHANGED" -eq 1 ]]; then
  SCOPE_ARGS+=(--changed)
fi
if [[ "${#PACKAGES[@]}" -gt 0 ]]; then
  for p in "${PACKAGES[@]}"; do
    SCOPE_ARGS+=(--package "$p")
  done
fi

if [[ "${#SCOPE_ARGS[@]}" -eq 0 ]]; then
  echo "bench-package: pass package name(s) or --changed" >&2
  exit 2
fi

SCOPE_JSON="$(python3 "$ROOT/benchmarks/harness/bench_scope.py" "${SCOPE_ARGS[@]}" --json)"
BENCHES="$(python3 -c "import json,sys; d=json.load(sys.stdin); print(','.join(d['benches']))" <<<"$SCOPE_JSON")"
PKGS="$(python3 -c "import json,sys; d=json.load(sys.stdin); print(' '.join(d['packages']))" <<<"$SCOPE_JSON")"

li_phase "bench scope: packages=[$PKGS] benches=[$BENCHES]"

if [[ -z "$BENCHES" && "$CHANGED" -eq 1 ]]; then
  li_phase "no tier benches in scope — sim/composable hooks only"
fi

# Sim / registry hooks + composable builds
if python3 -c "import json,sys; d=json.load(sys.stdin); sys.exit(0 if d.get('hooks') or d.get('composable') else 1)" <<<"$SCOPE_JSON"; then
  SIM_ARGS=(python3 "$ROOT/benchmarks/harness/bench_sim.py")
  [[ "${#PACKAGES[@]}" -gt 0 ]] && SIM_ARGS+=(--package "${PACKAGES[@]}")
  [[ "$CHANGED" -eq 1 ]] && SIM_ARGS+=(--changed)
  [[ "$WRITE_SUMMARY" -eq 1 ]] && SIM_ARGS+=(--write-summary)
  [[ "$VERIFY" -eq 0 ]] && SIM_ARGS+=(--skip-tier2)
  "${SIM_ARGS[@]}"
fi

# Package-scoped li-tests composables
if [[ -x "$LIC" && -n "$PKGS" ]]; then
  for pkg in $PKGS; do
    "$ROOT/li-tests/run_all.sh" --package "$pkg" composable
  done
fi

# Timings (merge into latest.csv for listed benches only)
if [[ "$TIMING" -eq 1 && -n "$BENCHES" ]]; then
  if [[ ! -x "$LIC" ]]; then
    echo "bench-package: skip timing (lic missing)" >&2
    exit 0
  fi
  ONLY_ARGS=(--only "$BENCHES")
  T1="$(python3 -c "import json,sys; b=set(json.load(sys.stdin)['benches']); print('1' if b & {'simd_dot','matmul_naive','matmul_blocked','reduce_sum','horner_pure_li'} else '0')" <<<"$SCOPE_JSON")"
  T2="$(python3 -c "import json,sys; b=set(json.load(sys.stdin)['benches']); print('1' if b - {'simd_dot','matmul_naive','matmul_blocked','reduce_sum','horner_pure_li'} else '0')" <<<"$SCOPE_JSON")"
  if [[ "$T1" == "1" ]]; then
  python3 "$ROOT/benchmarks/harness/bench.py" --tier 1 --runs "$RUNS" "${ONLY_ARGS[@]}" \
    $([[ "$VERIFY" -eq 0 ]] && echo --skip-verify)
  fi
  if [[ "$T2" == "1" ]]; then
  python3 "$ROOT/benchmarks/harness/bench.py" --tier 2 --runs "$RUNS" "${ONLY_ARGS[@]}" \
    $([[ "$VERIFY" -eq 0 ]] && echo --skip-verify)
  fi
fi

echo "bench-package: done"
