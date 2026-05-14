#!/usr/bin/env bash
# Optional CI bench smoke: tier-1 micro timings (no julia required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LIC="$ROOT/build/compiler/lic/lic"

if [[ ! -x "$LIC" ]]; then
  echo "ci-bench: run scripts/ci.sh first" >&2
  exit 1
fi

python3 "$ROOT/benchmarks/harness/bench.py" --tier 1 --runs 1 --skip-verify
echo "ci-bench: ok"
