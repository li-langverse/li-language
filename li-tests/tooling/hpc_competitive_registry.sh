#!/usr/bin/env bash
# Gate: competitive registry schema + optional CSV column hints.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
chmod +x "$ROOT/scripts/check-hpc-competitive.sh"
export LI_HPC_COMPETITIVE_STRICT="${LI_HPC_COMPETITIVE_STRICT:-0}"
"$ROOT/scripts/check-hpc-competitive.sh"
echo "hpc_competitive_registry: ok"
