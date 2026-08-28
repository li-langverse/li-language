#!/usr/bin/env bash
# Overnight: close all server parity milestones until 08:00 in HTTPD_PLAN_TZ (default Europe/Berlin).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/data/httpd-plan-loop"
mkdir -p "$LOG_DIR"
STAMP="$(date -u +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/overnight-${STAMP}.log"

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

HTTPD_PLAN_TZ="${HTTPD_PLAN_TZ:-Europe/Berlin}"
export TZ="$HTTPD_PLAN_TZ"

echo "==> httpd-plan-overnight $(date -Iseconds) TZ=$TZ → until ${HTTPD_PLAN_UNTIL_LOCAL:-08:00}"
echo "    close_server_milestones=$LI_HTTPD_PLAN_CLOSE_SERVER_MILESTONES log=$LOG"

if [[ "${HTTPD_PLAN_NO_UNTIL_DEADLINE:-0}" == "1" ]]; then
  MAX="${HTTPD_PLAN_OVERNIGHT_MAX:-30}"
  exec python3 "$ROOT/scripts/httpd-plan-loop.py" --max "$MAX" 2>&1 | tee -a "$LOG"
fi

export HTTPD_PLAN_WAIT_FOR_LOOP="${HTTPD_PLAN_WAIT_FOR_LOOP:-0}"
exec "$ROOT/scripts/httpd-plan-until-deadline.sh" 2>&1 | tee -a "$LOG"
