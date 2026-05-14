#!/usr/bin/env bash
# CI entry: build lic, li-tests, tier-0 benchmarks (verify + stability).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LIC="$ROOT/build/compiler/lic/lic"
export CC="${CC:-clang}"
export CXX="${CXX:-clang++}"

echo "==> build"
"$ROOT/scripts/build.sh"

if [[ ! -x "$LIC" ]]; then
  echo "ci: lic missing at $LIC" >&2
  exit 1
fi

echo "==> tier 0 (li-tests + verify + stability)"
python3 "$ROOT/benchmarks/harness/bench.py" --tier 0

echo "ci: ok"
