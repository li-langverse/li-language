#!/usr/bin/env bash
# afterFileEdit — official packages must have .github/workflows/ci.yml
set -euo pipefail
input="$(cat)"
path="$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('file_path',''))" 2>/dev/null || true)"
case "$path" in
  packages/*) ;;
  *) exit 0 ;;
esac

ROOT="${CURSOR_PROJECT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}"
# Resolve package root (packages/<name>/...)
rest="${path#packages/}"
name="${rest%%/*}"
[[ -n "$name" ]] || exit 0
PKG_DIR="$ROOT/packages/$name"
[[ -f "$PKG_DIR/PUBLISH.md" ]] || exit 0

if [[ ! -f "$PKG_DIR/.github/workflows/ci.yml" ]]; then
  echo "BLOCK: package $name missing .github/workflows/ci.yml" >&2
  echo "  run: ./scripts/ensure-package-ci.sh  or  ./scripts/li-new-package $name --official --force" >&2
  exit 2
fi
exit 0
