#!/usr/bin/env bash
# lic validate-httpd-config — M1 wrapper (Python schema until full compiler surface).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PYTHONPATH="${ROOT}/scripts${PYTHONPATH:+:$PYTHONPATH}"
exec python3 "${ROOT}/scripts/validate-httpd-config.py" "$@"
