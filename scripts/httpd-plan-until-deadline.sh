#!/usr/bin/env bash
# Run httpd-plan-loop in batches until a local-time deadline (default 08:00).
# Use after a fixed --max batch finishes early, or as the main overnight driver.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/data/httpd-plan-loop"
mkdir -p "$LOG_DIR"
STAMP="$(date -u +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/until-deadline-${STAMP}.log"

ENV_FILE="${LI_CURSOR_ENV_FILE:-$HOME/Documents/Cursor/.env}"
if [[ -f "$ENV_FILE" ]]; then
  set -a
  # shellcheck source=/dev/null
  source "$ENV_FILE"
  set +a
fi

export PATH="${HOME}/.local/node/bin:${PATH:-/usr/bin:/bin}"
export LI_CURSOR_AGENTS_ROOT="${LI_CURSOR_AGENTS_ROOT:-$ROOT/../li-cursor-agents}"
export BENCHMARKS_ROOT="${BENCHMARKS_ROOT:-$ROOT/../benchmarks}"
export LIC_ROOT="$ROOT"
export HTTPD_PLAN_PR_BRANCH="${HTTPD_PLAN_PR_BRANCH:-cursor/httpd-plan-continue}"
export HTTPD_GATES_SKIP_LIC_BUILD="${HTTPD_GATES_SKIP_LIC_BUILD:-1}"
export HTTPD_RUN_BEARER_TEST="${HTTPD_RUN_BEARER_TEST:-0}"
export HTTPD_REFRESH_PAGES="${HTTPD_REFRESH_PAGES:-1}"
export HTTPD_PAGES_SKIP_BENCH="${HTTPD_PAGES_SKIP_BENCH:-1}"
export LI_HTTPD_PLAN_AGENT="${LI_HTTPD_PLAN_AGENT:-code_implementer}"
export LI_HTTPD_PLAN_AGENT_TIMEOUT_SEC="${LI_HTTPD_PLAN_AGENT_TIMEOUT_SEC:-2700}"
export LI_CONTROL_PLANE_STORE="${LI_CONTROL_PLANE_STORE:-disk}"
export LI_HTTPD_PLAN_CLOSE_SERVER_MILESTONES="${LI_HTTPD_PLAN_CLOSE_SERVER_MILESTONES:-1}"

# Wall-clock deadline in operator TZ (default Europe/Berlin for 08:00 operator time).
HTTPD_PLAN_TZ="${HTTPD_PLAN_TZ:-Europe/Berlin}"
export TZ="$HTTPD_PLAN_TZ"
DEADLINE_LOCAL="${HTTPD_PLAN_UNTIL_LOCAL:-08:00}"
BATCH_CAP="${HTTPD_PLAN_BATCH_CAP:-30}"
MIN_BATCH="${HTTPD_PLAN_MIN_BATCH:-1}"
# Minutes budget per iteration (agent + gates + pages); tune from state.json history.
MIN_PER_ITER="${HTTPD_PLAN_MIN_PER_ITER:-12}"
WAIT_FOR_LOOP="${HTTPD_PLAN_WAIT_FOR_LOOP:-1}"

deadline_epoch() {
  local hm="$1"
  local h="${hm%%:*}"
  local m="${hm#*:}"
  m="${m%%:*}"
  local now today tomorrow
  now="$(date +%s)"
  today="$(date -d "today ${h}:${m}:00" +%s)"
  if [[ "$today" -gt "$now" ]]; then
    echo "$today"
    return
  fi
  tomorrow="$(date -d "tomorrow ${h}:${m}:00" +%s)"
  echo "$tomorrow"
}

log() { echo "$@" | tee -a "$LOG"; }

DEADLINE_TS="$(deadline_epoch "$DEADLINE_LOCAL")"

log "==> httpd-plan-until-deadline $(date -Iseconds)"
log "    TZ=${TZ} deadline=${DEADLINE_LOCAL} → $(date -d "@${DEADLINE_TS}" -Iseconds)"
log "    server_utc=$(date -u -Iseconds)"
log "    close_server_milestones=$LI_HTTPD_PLAN_CLOSE_SERVER_MILESTONES"
log "    batch_cap=$BATCH_CAP min_per_iter=${MIN_PER_ITER}m log=$LOG"

if [[ "$WAIT_FOR_LOOP" == "1" ]]; then
  log "==> waiting for any in-flight httpd-plan-loop.py to finish…"
  while pgrep -f 'httpd-plan-loop.py' >/dev/null 2>&1; do
    sleep 30
  done
  log "==> prior loop finished; starting deadline batches"
fi

cd "$ROOT"
BATCH_N=0
while true; do
  now="$(date +%s)"
  if [[ "$now" -ge "$DEADLINE_TS" ]]; then
    log "==> deadline reached $(date -Iseconds) — stop"
    break
  fi
  remaining_sec=$((DEADLINE_TS - now))
  if [[ "$remaining_sec" -lt 300 ]]; then
    log "==> <5m left — stop ($(date -Iseconds))"
    break
  fi
  remaining_min=$((remaining_sec / 60))
  batch=$((remaining_min / MIN_PER_ITER))
  if [[ "$batch" -lt "$MIN_BATCH" ]]; then
    batch="$MIN_BATCH"
  fi
  if [[ "$batch" -gt "$BATCH_CAP" ]]; then
    batch="$BATCH_CAP"
  fi
  BATCH_N=$((BATCH_N + 1))
  log ""
  log "==> batch ${BATCH_N} @ $(date -Iseconds) — --max ${batch} (~${remaining_min}m until deadline)"
  set +e
  python3 "$ROOT/scripts/httpd-plan-loop.py" --max "$batch" 2>&1 | tee -a "$LOG"
  rc=${PIPESTATUS[0]}
  set -e
  log "==> batch ${BATCH_N} exit ${rc} @ $(date -Iseconds)"
  if [[ "$rc" -ne 0 ]]; then
    log "==> non-zero exit; pause 90s then retry if before deadline"
    sleep 90
  else
    sleep 15
  fi
done

log "==> until-deadline done $(date -Iseconds)"
