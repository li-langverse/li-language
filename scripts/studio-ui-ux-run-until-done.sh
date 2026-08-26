#!/usr/bin/env bash
# Run studio_ui_ux_builder until plan todos done AND UX self-assessment passes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$ROOT/data/studio-ui-ux-plan-loop"
mkdir -p "$LOG_DIR"
STAMP="$(date -u +%Y%m%d-%H%M%S)"
LOG="$LOG_DIR/runner-${STAMP}.log"
ln -sf "$(basename "$LOG")" "$LOG_DIR/runner.log"

ENV_FILE="${LI_CURSOR_ENV_FILE:-$HOME/Documents/Cursor/.env}"
[[ -f "$ENV_FILE" ]] && set -a && source "$ENV_FILE" && set +a

export PATH="${HOME}/.local/node/bin:${PATH:-/usr/bin:/bin}"
export LI_CURSOR_AGENTS_ROOT="${LI_CURSOR_AGENTS_ROOT:-$ROOT/../li-cursor-agents}"
export STUDIO_UI_UX_PR_BRANCH="${STUDIO_UI_UX_PR_BRANCH:-cursor/studio-ui-ux-plan-loop}"
export STUDIO_UI_UX_PLAN_AGENT="${STUDIO_UI_UX_PLAN_AGENT:-studio_ui_ux_builder}"
export STUDIO_UI_UX_AGENT_TIMEOUT_SEC="${STUDIO_UI_UX_AGENT_TIMEOUT_SEC:-3600}"
export LI_SDK_TERMINAL_STREAM="${LI_SDK_TERMINAL_STREAM:-1}"

log() { echo "$@" | tee -a "$LOG"; }

pending_todos() {
  python3 - <<'PY'
import json, re
from pathlib import Path
root = Path(".")
plan = (root / "docs/superpowers/plans/2026-05-24-studio-ui-ux-plan-loop.md").read_text()
ids = re.findall(r"- id: (studio-ux-\S+)", plan)
state = json.loads((root / "data/studio-ui-ux-plan-loop/state.json").read_text()) if (root / "data/studio-ui-ux-plan-loop/state.json").is_file() else {}
done = set(state.get("completed_ids", []))
print(len([i for i in ids if i not in done]))
PY
}

ux_pass() {
  python3 - <<'PY'
import json
from pathlib import Path
p = Path("data/studio-ui-ux-plan-loop/latest-ux-assessment.json")
if not p.is_file():
    print("0")
    raise SystemExit
u = json.loads(p.read_text())
print("1" if u.get("pass") else "0")
PY
}

log "==> studio-ui-ux-run-until-done $(date -Iseconds)"
log "    branch=$STUDIO_UI_UX_PR_BRANCH agent=$STUDIO_UI_UX_PLAN_AGENT"

cd "$ROOT"
git fetch origin "$STUDIO_UI_UX_PR_BRANCH" 2>/dev/null || true
git checkout -B "$STUDIO_UI_UX_PR_BRANCH" "origin/${STUDIO_UI_UX_PR_BRANCH}" 2>/dev/null \
  || git checkout -B "$STUDIO_UI_UX_PR_BRANCH"

./scripts/studio-ui-ux-generate-design-system.sh 2>&1 | tee -a "$LOG" || true

FAIL_STREAK=0
MAX_FAIL="${STUDIO_UI_UX_MAX_FAIL_STREAK:-5}"

while true; do
  left="$(pending_todos)"
  up="$(ux_pass 2>/dev/null || echo 0)"
  if [[ "$left" -eq 0 ]] && [[ "$up" == "1" ]]; then
    log "==> DONE: all todos + UX gate pass"
    ./scripts/studio-ui-ux-daily-report.sh | tee -a "$LOG"
    exit 0
  fi
  log "==> pending todos=$left ux_pass=$up"

  if ! python3 "$ROOT/scripts/studio-ui-ux-plan-loop.py" --once 2>&1 | tee -a "$LOG"; then
    FAIL_STREAK=$((FAIL_STREAK + 1))
    log "==> iteration failed (streak=$FAIL_STREAK)"
    if [[ "$FAIL_STREAK" -ge "$MAX_FAIL" ]]; then
      log "==> stop: max fail streak $MAX_FAIL"
      ./scripts/studio-ui-ux-daily-report.sh | tee -a "$LOG" || true
      exit 1
    fi
    sleep 120
    continue
  fi
  FAIL_STREAK=0
  sleep 45
done
