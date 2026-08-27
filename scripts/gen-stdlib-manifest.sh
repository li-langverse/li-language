#!/usr/bin/env bash
# Print top-level def/proc/type names from std/**/*.li for prelude.cpp sync.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STD="$ROOT/std"
if [[ ! -d "$STD" ]]; then
  echo "no std/ directory" >&2
  exit 1
fi
rg -n '^(def|proc|type|extern proc) ' "$STD" --glob '*.li' 2>/dev/null || true
