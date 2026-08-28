#!/usr/bin/env bash
# Remind to refresh competitive registry after benchmark / parallel edits.
# Cursor: afterFileEdit (benchmarks) or stop (session end).
set -euo pipefail
ROOT="${CURSOR_PROJECT_DIR:-$(pwd)}"
REGISTRY="$ROOT/benchmarks/competitive/registry.toml"
[[ -f "$REGISTRY" ]] || exit 0

if ! git -C "$ROOT" rev-parse --is-inside-work-tree &>/dev/null; then
  exit 0
fi

changed="$(git -C "$ROOT" diff --name-only HEAD 2>/dev/null; git -C "$ROOT" diff --name-only --cached 2>/dev/null)" || exit 0
[[ -n "$changed" ]] || exit 0

touch_registry=0
for f in $changed; do
  case "$f" in
    benchmarks/*|compiler/*parallel*|compiler/*simd*|compiler/*openmp*)
      touch_registry=1
      ;;
  esac
done
[[ "$touch_registry" -eq 1 ]] || exit 0

if git -C "$ROOT" diff --name-only --cached -- "$REGISTRY" 2>/dev/null | grep -q . ||
  git -C "$ROOT" diff --name-only -- "$REGISTRY" 2>/dev/null | grep -q .; then
  exit 0
fi

echo "hpc-competitive: update benchmarks/competitive/registry.toml last_reviewed" >&2
echo "  run: ./scripts/check-hpc-competitive.sh" >&2
echo "  skill: hpc-competitive-review" >&2
exit 0
