#!/usr/bin/env bash
# Generate all X-ready PNGs: li-tests suites + benchmark CSV.
set -euo pipefail
export MPLBACKEND=Agg
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENV="${ROOT}/.venv-plot"
SHARE="${ROOT}/benchmarks/results/share"

if [[ ! -d "$VENV" ]]; then
  python3 -m venv "$VENV"
fi
# shellcheck disable=SC1091
source "$VENV/bin/activate"
pip install -q -r "${ROOT}/benchmarks/harness/requirements.txt"

python3 "${ROOT}/benchmarks/harness/bench.py" --tier 12 --runs 3 --skip-verify --out "${ROOT}/benchmarks/results/latest.csv" || \
  python3 "${ROOT}/benchmarks/harness/bench.py" --sample
python3 "${ROOT}/benchmarks/harness/stability.py" --out "${ROOT}/benchmarks/results/stability.csv" || true
python3 "${ROOT}/benchmarks/harness/trace_energy.py" --out "${ROOT}/benchmarks/results/md_lennard_jones" || true
python3 "${ROOT}/benchmarks/harness/verify.py" --write-csv "${ROOT}/benchmarks/results/verify.csv" || true
python3 "${ROOT}/benchmarks/harness/plot.py" --out "$SHARE" --energy-dir "${ROOT}/benchmarks/results/md_lennard_jones"
python3 "${ROOT}/li-tests/harness/plot_suites.py" --out "$SHARE"

echo "Shareable plots:"
ls -la "$SHARE"/*.png
