#!/usr/bin/env bash
# systemd ExecStart — single instance (flock), always-on after reboot.
# Runs until plan + UX pass, then idles and polls for new work.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOCK="${STUDIO_UI_UX_FLOCK:-/tmp/li-studio-ui-ux-plan-loop.lock}"
IDLE_SEC="${STUDIO_UI_UX_IDLE_SEC:-1800}"
ENV_FILE="${LI_CURSOR_ENV_FILE:-$HOME/Documents/Cursor/.env}"

if [[ -f "${ROOT}/data/studio-ui-ux-plan-loop/DISABLE_AUTOSTART" ]]; then
  echo "studio-ui-ux-systemd: DISABLE_AUTOSTART set — exit 0"
  exit 0
fi

if [[ -f "$ENV_FILE" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "$ENV_FILE"
  set +a
fi

export PATH="${HOME}/.local/node/bin:${PATH:-/usr/bin:/bin}"
export LI_CURSOR_AGENTS_ROOT="${LI_CURSOR_AGENTS_ROOT:-$ROOT/../li-cursor-agents}"
export LIC_ROOT="$ROOT"

exec 9>"$LOCK"
if ! flock -n 9; then
  echo "studio-ui-ux-systemd: another instance holds $LOCK — exit 0"
  exit 0
fi

cd "$ROOT"
while true; do
  if [[ -f "${ROOT}/data/studio-ui-ux-plan-loop/DISABLE_AUTOSTART" ]]; then
    echo "studio-ui-ux-systemd: DISABLE_AUTOSTART — stopping"
    exit 0
  fi
  if "$ROOT/scripts/studio-ui-ux-run-until-done.sh"; then
    echo "studio-ui-ux-systemd: plan + UX complete — idle ${IDLE_SEC}s"
    sleep "$IDLE_SEC"
  else
    echo "studio-ui-ux-systemd: runner failed — retry in 300s"
    sleep 300
  fi
done
