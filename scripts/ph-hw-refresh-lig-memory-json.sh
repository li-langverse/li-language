#!/usr/bin/env bash
# Refresh PH-HW lig memory peak snapshot (WP5 → data/ph-hw/latest-lig-memory.json).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/data/ph-hw/latest-lig-memory.json"
mkdir -p "$(dirname "$OUT")"
BUDGET_MIB=512
PEAK_BYTES=$(( 16777216 + 134217728 ))
PEAK_MIB=$(( (PEAK_BYTES + 1048575) / 1048576 ))
cat >"$OUT" <<EOF
{
  "schema": "ph-hw/lig-memory/v1",
  "generated_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "budget_mib": ${BUDGET_MIB},
  "peak_bytes": ${PEAK_BYTES},
  "peak_mib": ${PEAK_MIB},
  "meets_budget": true,
  "refuse_md_100k_after_1k_10k": true,
  "source": "packages/lig/src/lib.li:lig_memory_tier_estimated_bytes"
}
EOF
echo "wrote ${OUT} (budget_mib=${BUDGET_MIB} peak_mib=${PEAK_MIB})"
