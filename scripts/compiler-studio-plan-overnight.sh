#!/usr/bin/env bash
# Overnight: compiler+Studio plan loop until 08:00 in HTTPD_PLAN_TZ (default Europe/Berlin).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/data/compiler-studio-plan-loop"
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
export COMPILER_STUDIO_PR_BRANCH="${COMPILER_STUDIO_PR_BRANCH:-cursor/compiler-studio-plan-loop}"
export COMPILER_STUDIO_GATES_SKIP_LIC_BUILD="${COMPILER_STUDIO_GATES_SKIP_LIC_BUILD:-0}"
export COMPILER_STUDIO_REFRESH_PAGES="${COMPILER_STUDIO_REFRESH_PAGES:-0}"
export LI_COMPILER_STUDIO_PLAN_AGENT="${LI_COMPILER_STUDIO_PLAN_AGENT:-code_implementer}"
export LI_COMPILER_STUDIO_PLAN_AGENT_TIMEOUT_SEC="${LI_COMPILER_STUDIO_PLAN_AGENT_TIMEOUT_SEC:-2700}"
export LI_CONTROL_PLANE_STORE="${LI_CONTROL_PLANE_STORE:-disk}"

HTTPD_PLAN_TZ="${HTTPD_PLAN_TZ:-Europe/Berlin}"
export TZ="$HTTPD_PLAN_TZ"

echo "==> compiler-studio-plan-overnight $(date -Iseconds) TZ=$TZ → until ${HTTPD_PLAN_UNTIL_LOCAL:-08:00}"
echo "    branch=$COMPILER_STUDIO_PR_BRANCH log=$LOG"

if [[ "${HTTPD_PLAN_NO_UNTIL_DEADLINE:-0}" == "1" ]]; then
  MAX="${HTTPD_PLAN_OVERNIGHT_MAX:-30}"
  exec python3 "$ROOT/scripts/compiler-studio-plan-loop.py" --max "$MAX" 2>&1 | tee -a "$LOG"
fi

export HTTPD_PLAN_WAIT_FOR_LOOP="${HTTPD_PLAN_WAIT_FOR_LOOP:-0}"
exec "$ROOT/scripts/compiler-studio-plan-until-deadline.sh" 2>&1 | tee -a "$LOG"
