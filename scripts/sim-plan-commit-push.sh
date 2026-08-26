#!/usr/bin/env bash
# Commit iteration artifacts and push SIM_PLAN_PR_BRANCH (plan loop recovery).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BRANCH="${SIM_PLAN_PR_BRANCH:-cursor/sim-algo-plan-loop}"
TODO_ID="${1:-sim-iteration}"
MSG="${2:-feat(sim): ${TODO_ID} — algo iteration}"

cd "$ROOT"
if [[ "$(git branch --show-current)" != "$BRANCH" ]]; then
  git fetch origin "$BRANCH" 2>/dev/null || true
  git checkout -B "$BRANCH" "origin/${BRANCH}" 2>/dev/null || git checkout -B "$BRANCH"
fi

git add -A
if git diff --cached --quiet; then
  echo "sim-plan-commit-push: nothing to commit"
else
  git commit -m "$MSG"
fi

TOKEN="${GH_TOKEN:-${GITHUB_TOKEN:-}}"
if [[ -z "$TOKEN" ]]; then
  echo "sim-plan-commit-push: no GH_TOKEN — skip push" >&2
  exit 0
fi
git push "https://x-access-token:${TOKEN}@github.com/li-langverse/lic.git" "HEAD:${BRANCH}"
echo "sim-plan-commit-push: pushed ${BRANCH}"
