#!/usr/bin/env bash
# Sample peak RSS for scoped native bench binaries (/usr/bin/time -v).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BENCHES="${1:-}"
OUT_DIR="${ROOT}/benchmarks/results/memory"
mkdir -p "$OUT_DIR"

if [[ -z "$BENCHES" ]]; then
  BENCHES="$(python3 "$ROOT/benchmarks/harness/bench_scope.py" --package "${SIM_PLAN_PACKAGE:-li-sim-scientific}" --print-benches 2>/dev/null || true)"
fi

if [[ -z "$BENCHES" ]]; then
  echo "sim-bench-memory: no benches in scope" >&2
  exit 0
fi

SUMMARY="${OUT_DIR}/latest_memory.json"
python3 - <<'PY' > "$SUMMARY.tmp"
import json, os
from pathlib import Path
print(json.dumps({"schema": "li_sim_memory_v1", "samples": []}))
PY

IFS=',' read -ra IDS <<< "$BENCHES"
for id in "${IDS[@]}"; do
  id="${id// /}"
  [[ -z "$id" ]] && continue
  bin="${ROOT}/build/bench/${id}/${id}_native"
  if [[ ! -x "$bin" ]]; then
    echo "sim-bench-memory: skip $id (no ${bin})"
    continue
  fi
  log="${OUT_DIR}/${id}_native.time.txt"
  /usr/bin/time -v "$bin" > /dev/null 2> "$log" || true
  rss="$(grep -E '^Maximum resident set size' "$log" | awk '{print $6}' || echo 0)"
  echo "sim-bench-memory: $id peak_rss_kb=${rss:-0}"
  python3 - <<PY
import json
from pathlib import Path
p = Path("$SUMMARY.tmp")
doc = json.loads(p.read_text())
doc["samples"].append({"benchmark": "$id", "peak_rss_kb": int("${rss:-0}")})
p.write_text(json.dumps(doc, indent=2) + "\n")
PY
done
mv "$SUMMARY.tmp" "$SUMMARY"
echo "sim-bench-memory: wrote $SUMMARY"
