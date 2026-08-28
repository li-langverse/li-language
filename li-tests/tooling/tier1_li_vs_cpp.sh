#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
chmod +x "$ROOT/scripts/check-tier1-li-vs-cpp.sh"
export LI_TIER1_PERF_STRICT="${LI_TIER1_PERF_STRICT:-0}"
"$ROOT/scripts/check-tier1-li-vs-cpp.sh"
echo "tier1_li_vs_cpp: ok"
