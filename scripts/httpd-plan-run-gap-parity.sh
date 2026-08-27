#!/usr/bin/env bash
# Run gap-parity plan todos after any in-flight httpd plan agent exits.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p data/httpd-plan-loop

ENV_FILE="${LI_CURSOR_ENV_FILE:-$HOME/Documents/Cursor/.env}"
if [[ -f "$ENV_FILE" ]]; then
  set -a
  # shellcheck source=/dev/null
  source "$ENV_FILE"
  set +a
fi

if [[ -f data/httpd-plan-loop/env.sh ]]; then
  # shellcheck source=/dev/null
  source data/httpd-plan-loop/env.sh
else
  export PATH="${HOME}/.local/node/bin:${PATH}"
  export LI_CURSOR_AGENTS_ROOT="${LI_CURSOR_AGENTS_ROOT:-$ROOT/../li-cursor-agents}"
  export BENCHMARKS_ROOT="${BENCHMARKS_ROOT:-$ROOT/../benchmarks}"
  export HTTPD_PLAN_PR_BRANCH="${HTTPD_PLAN_PR_BRANCH:-cursor/httpd-plan-continue}"
  export LI_REPO_WORKFLOW_BRANCH="${HTTPD_PLAN_PR_BRANCH}"
  export LI_REPO_WORKFLOW_TRACK_REMOTE=1
  export LI_HTTPD_PLAN_CLOSE_SERVER_MILESTONES=1
fi

export HTTPD_GATES_SKIP_LIC_BUILD="${HTTPD_GATES_SKIP_LIC_BUILD:-1}"
export LI_HTTPD_PLAN_AGENT="${LI_HTTPD_PLAN_AGENT:-code_implementer}"
export LI_HTTPD_PLAN_AGENT_TIMEOUT_SEC="${LI_HTTPD_PLAN_AGENT_TIMEOUT_SEC:-2700}"

LOG="data/httpd-plan-loop/gap-parity-$(date -u +%Y%m%d-%H%M%S).log"
MAX="${HTTPD_PLAN_GAP_MAX:-6}"

echo "==> httpd-plan-run-gap-parity $(date -Iseconds) max=$MAX log=$LOG"

wait_idle() {
  while pgrep -f 'run-agent.js.*code_implementer.*httpd-plan-loop' >/dev/null 2>&1 \
    || pgrep -f 'httpd-plan-loop.py' >/dev/null 2>&1; do
    echo "    waiting for in-flight httpd plan agent..."
    sleep 30
  done
}

wait_idle
git fetch origin "${HTTPD_PLAN_PR_BRANCH:-cursor/httpd-plan-continue}"
git pull --ff-only origin "${HTTPD_PLAN_PR_BRANCH:-cursor/httpd-plan-continue}" || true

exec python3 "$ROOT/scripts/httpd-plan-loop.py" --max "$MAX" 2>&1 | tee -a "$LOG"
