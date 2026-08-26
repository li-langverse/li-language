#!/usr/bin/env bash
# Cursor afterFileEdit — nudge when std/ changes.
set -euo pipefail
input="$(cat)"
path="$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('file_path',''))" 2>/dev/null || true)"
[[ "$path" == *std/* ]] || exit 0
echo "reminder: std/ requires 100% line coverage + li-tests fixture + gen-stdlib-manifest.sh if exports changed" >&2
exit 0
