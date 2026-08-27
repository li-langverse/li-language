#!/usr/bin/env bash
# Verify benchmark numerical results (Li vs native --verify). No timing sweep.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"

export CC="${CC:-clang-22}"
export CXX="${CXX:-clang++-22}"
export LIC="${LIC:-$ROOT/build/compiler/lic/lic}"

if [[ ! -x "$LIC" ]]; then
  li_fail "missing lic — run ./scripts/build.sh"
  exit 1
fi

TIER="${1:-12}"
li_phase "benchmark result verify (tier ${TIER}, no timing)"
python3 "$ROOT/benchmarks/harness/bench.py" --verify-results --tier "$TIER"
li_ok "benchmark result verify finished"
