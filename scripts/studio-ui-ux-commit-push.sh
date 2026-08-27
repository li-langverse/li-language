#!/usr/bin/env bash
# Commit iteration artifacts and push STUDIO_UI_UX_PR_BRANCH.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BRANCH="${STUDIO_UI_UX_PR_BRANCH:-cursor/studio-ui-ux-plan-loop}"
TODO_ID="${1:-studio-iteration}"
MSG="${2:-feat(studio-ui): ${TODO_ID} — UX iteration}"

cd "$ROOT"
if [[ "$(git branch --show-current)" != "$BRANCH" ]]; then
  git fetch origin "$BRANCH" 2>/dev/null || true
  git checkout -B "$BRANCH" "origin/${BRANCH}" 2>/dev/null || git checkout -B "$BRANCH"
fi

# Never stage capture binaries
git add -A
git reset -q HEAD -- 'deploy/studio-demo/screenshots/png/' 'data/studio-ui-ux-plan-loop/artifacts/' 2>/dev/null || true
git reset -q HEAD -- '*.mp4' 2>/dev/null || true

if git diff --cached --quiet; then
  echo "studio-ui-ux-commit-push: nothing to commit"
else
  git commit -m "$MSG"
fi

TOKEN="${GH_TOKEN:-${GITHUB_TOKEN:-}}"
if [[ -z "$TOKEN" ]]; then
  echo "studio-ui-ux-commit-push: no GH_TOKEN — skip push" >&2
  exit 0
fi
git push "https://x-access-token:${TOKEN}@github.com/li-langverse/lic.git" "HEAD:${BRANCH}"
echo "studio-ui-ux-commit-push: pushed ${BRANCH}"
