#!/usr/bin/env bash
# Daily 08:00 operator report — registry progress, last iterations, perf/memory snapshot.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV_FILE="${LI_CURSOR_ENV_FILE:-$HOME/Documents/Cursor/.env}"
if [[ -f "$ENV_FILE" ]]; then
  set -a
  # shellcheck source=/dev/null
  source "$ENV_FILE"
  set +a
fi

export TZ="${SIM_PLAN_TZ:-Europe/Berlin}"
DAY="$(date +%Y-%m-%d)"
OUT="${ROOT}/docs/reports/sim-plan/daily/${DAY}.md"
mkdir -p "$(dirname "$OUT")"

STATS="$(python3 - <<'PY'
import json
from pathlib import Path
r = Path("benchmarks/competitive/algo_registry.json")
d = json.loads(r.read_text())
algos = d["algorithms"]
impl = sum(1 for a in algos if a.get("implemented_smoke"))
print(f"{impl}|{len(algos)}")
PY
)"
IMPL="${STATS%%|*}"
TOTAL="${STATS##*|}"

STATE="${ROOT}/data/sim-plan-loop/state.json"
HISTORY_SNIP=""
if [[ -f "$STATE" ]]; then
  HISTORY_SNIP="$(python3 -c "
import json
from pathlib import Path
s=json.loads(Path('$STATE').read_text())
h=s.get('history',[])[-5:]
for row in h:
    print(f\"- {row.get('at','')}: {row.get('todo_id','')} exit={row.get('agent_exit')} gates={row.get('gates_ok')}\")
" 2>/dev/null || true)"
fi

BRANCH="$(git -C "$ROOT" branch --show-current 2>/dev/null || echo unknown)"
SHA="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"

{
  echo "# Sim plan — daily report ${DAY}"
  echo ""
  echo "_Generated $(date -Iseconds) (${TZ})_"
  echo ""
  echo "## Summary"
  echo ""
  echo "| Metric | Value |"
  echo "|--------|-------|"
  echo "| Algorithms with smoke | **${IMPL} / ${TOTAL}** |"
  echo "| Branch | \`${BRANCH}\` |"
  echo "| HEAD | \`${SHA}\` |"
  echo "| Runner | \`data/sim-plan-loop/runner.log\` |"
  echo ""
  echo "## Last iterations"
  echo ""
  if [[ -n "$HISTORY_SNIP" ]]; then
    echo "$HISTORY_SNIP"
  else
    echo "_No state.json history yet._"
  fi
  echo ""
  echo "## Live status"
  echo ""
  if [[ -f "${ROOT}/docs/reports/sim-plan/STATUS.md" ]]; then
    tail -n 20 "${ROOT}/docs/reports/sim-plan/STATUS.md"
  fi
  echo ""
  echo "## Performance (latest.csv excerpt)"
  echo ""
  if [[ -f "${ROOT}/benchmarks/results/latest.csv" ]]; then
    echo '```'
    head -1 "${ROOT}/benchmarks/results/latest.csv"
    rg -i 'md_lennard_jones|heat_equation|li,' "${ROOT}/benchmarks/results/latest.csv" 2>/dev/null | head -20 || true
    echo '```'
  else
    echo "_No latest.csv_"
  fi
  echo ""
  echo "## Memory"
  echo ""
  if [[ -f "${ROOT}/benchmarks/results/memory/latest_memory.json" ]]; then
    echo '```json'
    cat "${ROOT}/benchmarks/results/memory/latest_memory.json"
    echo '```'
  else
    echo "_No memory snapshot_"
  fi
} > "$OUT"

cp -f "$OUT" "${ROOT}/docs/reports/sim-plan/daily/LATEST.md"
echo "sim-plan-daily-report: $OUT"
# Append to rolling log for email/CI hooks
echo "$(date -Iseconds) daily ${IMPL}/${TOTAL} ${SHA}" >> "${ROOT}/data/sim-plan-loop/daily.log"
