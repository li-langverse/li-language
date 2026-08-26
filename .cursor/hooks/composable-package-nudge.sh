#!/usr/bin/env bash
# Cursor afterFileEdit — nudge when packages/*/src/main.li grows without lib.li API.
set -euo pipefail
input="$(cat)"
path="$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('file_path',''))" 2>/dev/null || true)"
case "$path" in
  packages/*/src/main.li) ;;
  *) exit 0 ;;
esac

ROOT="${CURSOR_PROJECT_DIR:-$(pwd)}"
pkg_dir="${path%/src/main.li}"
lib="$ROOT/$pkg_dir/src/lib.li"
if [[ ! -f "$lib" ]]; then
  echo "composable: add $pkg_dir/src/lib.li with exported lifecycle API (see docs/ecosystem/composable-by-default.md)" >&2
  echo "  skill: composable-li-library" >&2
  exit 0
fi
if ! grep -qE '^def ' "$lib" 2>/dev/null; then
  echo "composable: $lib has no exported def — move logic out of main.li" >&2
  exit 0
fi
exit 0
