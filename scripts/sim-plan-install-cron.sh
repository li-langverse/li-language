#!/usr/bin/env bash
# Install user crontab entry: daily sim report at 08:00 (SIM_PLAN_TZ).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TZ_NAME="${SIM_PLAN_TZ:-Europe/Berlin}"
HOUR="${SIM_PLAN_DAILY_HOUR:-8}"
MINUTE="${SIM_PLAN_DAILY_MINUTE:-0}"
LINE="${MINUTE} ${HOUR} * * * TZ=${TZ_NAME} ${ROOT}/scripts/sim-plan-daily-report.sh >> ${ROOT}/data/sim-plan-loop/daily-cron.log 2>&1"

mkdir -p "${ROOT}/data/sim-plan-loop"
(crontab -l 2>/dev/null | grep -v 'sim-plan-daily-report.sh' || true; echo "$LINE") | crontab -
echo "Installed crontab line:"
echo "  $LINE"
