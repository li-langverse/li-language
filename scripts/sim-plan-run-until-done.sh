#!/usr/bin/env bash
# Run goal-directed sim agent until all registry smokes implemented (or failure).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/data/sim-plan-loop"
mkdir -p "$LOG_DIR"
STAMP="$(date -u +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/runner-${STAMP}.log"
ln -sf "$(basename "$LOG")" "$LOG_DIR/runner.log"

ENV_FILE="${LI_CURSOR_ENV_FILE:-$HOME/Documents/Cursor/.env}"
if [[ -f "$ENV_FILE" ]]; then
  set -a
  # shellcheck source=/dev/null
  source "$ENV_FILE"
  set +a
fi

export PATH="${HOME}/.local/node/bin:${PATH:-/usr/bin:/bin}"
export LI_CURSOR_AGENTS_ROOT="${LI_CURSOR_AGENTS_ROOT:-$ROOT/../li-cursor-agents}"
export LIC_ROOT="$ROOT"
export SIM_PLAN_PR_BRANCH="${SIM_PLAN_PR_BRANCH:-cursor/sim-algo-plan-loop}"
export SIM_PLAN_PACKAGE="${SIM_PLAN_PACKAGE:-li-sim-scientific}"
export LI_SIM_PLAN_AGENT="${LI_SIM_PLAN_AGENT:-code_implementer}"
export LI_SIM_PLAN_AGENT_TIMEOUT_SEC="${LI_SIM_PLAN_AGENT_TIMEOUT_SEC:-3600}"
export LI_SDK_TERMINAL_STREAM="${LI_SDK_TERMINAL_STREAM:-1}"
export SIM_PLAN_BENCH_RUNS="${SIM_PLAN_BENCH_RUNS:-3}"

log() { echo "$@" | tee -a "$LOG"; }

remaining() {
  python3 - <<'PY'
import json
from pathlib import Path
d = json.loads(Path("benchmarks/competitive/algo_registry.json").read_text())
impl = sum(1 for a in d["algorithms"] if a.get("implemented_smoke"))
print(len(d["algorithms"]) - impl)
PY
}

log "==> sim-plan-run-until-done $(date -Iseconds)"
log "    branch=$SIM_PLAN_PR_BRANCH package=$SIM_PLAN_PACKAGE"
log "    log=$LOG"

cd "$ROOT"
git fetch origin "$SIM_PLAN_PR_BRANCH" 2>/dev/null || true
git checkout -B "$SIM_PLAN_PR_BRANCH" "origin/${SIM_PLAN_PR_BRANCH}" 2>/dev/null \
  || git checkout -B "$SIM_PLAN_PR_BRANCH"

FAIL_STREAK=0
MAX_FAIL="${SIM_PLAN_MAX_FAIL_STREAK:-5}"

while true; do
  left="$(remaining)"
  if [[ "$left" -eq 0 ]]; then
    log "==> DONE: all algorithms have implemented_smoke"
    ./scripts/sim-plan-daily-report.sh | tee -a "$LOG"
    exit 0
  fi
  log "==> remaining registry smokes: $left"
  if ! python3 "$ROOT/scripts/sim-plan-loop.py" --once 2>&1 | tee -a "$LOG"; then
    FAIL_STREAK=$((FAIL_STREAK + 1))
    log "==> iteration failed (streak=$FAIL_STREAK)"
    if [[ "$FAIL_STREAK" -ge "$MAX_FAIL" ]]; then
      log "==> stop: max fail streak $MAX_FAIL"
      exit 1
    fi
    sleep 120
    continue
  fi
  FAIL_STREAK=0
  ./scripts/sim-plan-commit-push.sh "sim-auto" "chore(sim): plan loop iteration $(date -u +%Y%m%dT%H%M%SZ)" \
    | tee -a "$LOG" || true
  sleep 30
done
