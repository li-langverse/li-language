#!/usr/bin/env bash
# Tier-1 math microbench: report Li vs C++ wall_time ratio; optional strict ≤1.2× gate.
# Uses benchmarks/results/latest.csv (run bench.py --tier 1 locally to refresh).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"

CSV="${LI_TIER1_PERF_CSV:-$ROOT/benchmarks/results/latest.csv}"
STRICT="${LI_TIER1_PERF_STRICT:-0}"
MAX_RATIO="${LI_TIER1_MAX_RATIO:-1.2}"

# Pure-Li math kernels under benchmarks/tier1_micro (exclude shared-C reduce_sum).
BENCHES=(
  simd_dot
  matmul_naive
  matmul_blocked
  horner_pure_li
)

li_phase "tier-1 Li vs C++ (max ${MAX_RATIO}×, strict=${STRICT})"

[[ -f "$CSV" ]] || {
  li_fail "missing $CSV — run: python3 benchmarks/harness/bench.py --tier 1"
  exit 1
}

export CSV MAX_RATIO STRICT
python3 - <<'PY'
from __future__ import annotations

import csv
import os
import sys
from pathlib import Path

csv_path = Path(os.environ["CSV"])
max_ratio = float(os.environ["MAX_RATIO"])
strict = os.environ.get("STRICT", "0") == "1"
benches = (
    "simd_dot",
    "matmul_naive",
    "matmul_blocked",
    "horner_pure_li",
)

rows: dict[tuple[str, str], float] = {}
with csv_path.open(newline="") as f:
    for row in csv.DictReader(f):
        if row.get("metric") != "wall_time":
            continue
        key = (row["benchmark"], row["lang"])
        rows[key] = float(row["value"])

errors: list[str] = []
gaps: list[str] = []
ok_rows: list[str] = []

for bench in benches:
    li_t = rows.get((bench, "li"))
    cpp_t = rows.get((bench, "cpp"))
    if li_t is None or cpp_t is None:
        errors.append(f"{bench}: missing li or cpp wall_time in {csv_path}")
        continue
    if cpp_t <= 0:
        errors.append(f"{bench}: invalid cpp wall_time {cpp_t}")
        continue
    ratio = li_t / cpp_t
    line = f"{bench}: li={li_t:.6f}s cpp={cpp_t:.6f}s ratio={ratio:.3f}× (cap {max_ratio}×)"
    if ratio > max_ratio:
        gaps.append(line)
    else:
        ok_rows.append(line)

print("tier1_li_vs_cpp: within cap:")
for line in ok_rows:
    print(f"  OK  {line}")
if gaps:
    print("tier1_li_vs_cpp: GAPS (Li slower than cap vs C++):")
    for line in gaps:
        print(f"  GAP {line}")

if errors:
    for e in errors:
        print(f"tier1_li_vs_cpp: ERROR {e}", file=sys.stderr)
    sys.exit(1)

if gaps and strict:
    print(f"tier1_li_vs_cpp: FAIL strict mode ({len(gaps)} bench(es) > {max_ratio}× C++)", file=sys.stderr)
    sys.exit(1)

if gaps:
    print(f"tier1_li_vs_cpp: advisory — {len(gaps)} gap(s); set LI_TIER1_PERF_STRICT=1 to fail CI")
else:
    print("tier1_li_vs_cpp: all listed tier-1 math benches within cap")
PY

li_ok "tier1_li_vs_cpp check finished"
