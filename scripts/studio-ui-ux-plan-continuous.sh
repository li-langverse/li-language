#!/usr/bin/env bash
# Run Studio UI/UX plan loop continuously; idle when no todos.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOOP="$ROOT/scripts/studio-ui-ux-plan-loop.py"
IDLE_SEC="${STUDIO_UI_UX_IDLE_SEC:-1800}"
LOCK="${STUDIO_UI_UX_FLOCK:-/tmp/li-studio-ui-ux-plan-loop.lock}"

if [[ -f "$HOME/Documents/Cursor/.env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "$HOME/Documents/Cursor/.env"
  set +a
fi

export LI_CURSOR_AGENTS_ROOT="${LI_CURSOR_AGENTS_ROOT:-$ROOT/../li-cursor-agents}"

exec 9>"$LOCK"
if ! flock -n 9; then
  echo "studio-ui-ux: another loop holds $LOCK"
  exit 0
fi

while true; do
  if ! python3 "$LOOP" --once; then
    rc=$?
    if [[ "$rc" -eq 0 ]]; then
      echo "studio-ui-ux: idle ${IDLE_SEC}s (no todos or done)"
      sleep "$IDLE_SEC"
      continue
    fi
    echo "studio-ui-ux: loop exit $rc — sleep 300s"
    sleep 300
    continue
  fi
  sleep 5
done
