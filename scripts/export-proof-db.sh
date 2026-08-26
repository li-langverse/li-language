#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export ROOT
exec python3 "$ROOT/scripts/export-proof-db.py"
