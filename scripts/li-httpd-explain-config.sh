#!/usr/bin/env bash
# M1: print canonical desugared [routes] (oracle: httpd_config.py --explain).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ $# -lt 1 ]]; then
  echo "usage: li-httpd-explain-config.sh <config.toml>" >&2
  exit 2
fi
export PYTHONPATH="${ROOT}/scripts${PYTHONPATH:+:$PYTHONPATH}"
exec python3 "${ROOT}/scripts/httpd_config.py" "$1" --explain
