#!/usr/bin/env bash
# Cursor stop — remind to open PR (no direct push to main/dev).
set -euo pipefail
LI_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if [[ "${LI_SKIP_OPEN_PR_REMINDER:-}" == "1" ]]; then
  exit 0
fi
if git -C "$LI_ROOT" rev-parse --is-inside-work-tree &>/dev/null; then
  branch="$(git -C "$LI_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || true)"
  if [[ "$branch" != "main" && "$branch" != "dev" && -n "$branch" ]]; then
    echo "PR workflow: push branch '$branch' and open PR — do not merge your own PR" >&2
    echo "  git push -u origin HEAD && gh pr create --fill" >&2
  fi
fi
exit 0
