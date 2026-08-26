#!/usr/bin/env bash
# Optional: daily markdown archive at 08:00. Live canvases use agent-canvases-watch (systemd).
# Crontab: daily Studio UI/UX report + canvas refresh at 08:00 (STUDIO_UI_UX_TZ).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TZ_NAME="${STUDIO_UI_UX_TZ:-Europe/Berlin}"
HOUR="${STUDIO_UI_UX_DAILY_HOUR:-8}"
MINUTE="${STUDIO_UI_UX_DAILY_MINUTE:-0}"
LINE="${MINUTE} ${HOUR} * * * TZ=${TZ_NAME} ${ROOT}/scripts/studio-ui-ux-daily-report.sh >> ${ROOT}/data/studio-ui-ux-plan-loop/daily-cron.log 2>&1"

mkdir -p "${ROOT}/data/studio-ui-ux-plan-loop"
(crontab -l 2>/dev/null | grep -v 'studio-ui-ux-daily-report.sh' || true; echo "$LINE") | crontab -
echo "Installed:"
echo "  $LINE"
