#!/usr/bin/env bash
# Cursor afterFileEdit — remind vision placement when ecosystem/plan docs change.
set -euo pipefail
input="$(cat)"
path="$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('file_path',''))" 2>/dev/null || true)"
case "$path" in
  *docs/ecosystem/*|*docs/superpowers/plans/*)
    echo "reminder: cross-repo/pillar vision -> master plan; package-only -> README/PUBLISH; see vision-and-roadmap.md" >&2
    ;;
esac
exit 0
