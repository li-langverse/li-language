#!/usr/bin/env bash
# Commit (if needed) and push lic monorepo — uses ../.env.github via with-github-env.sh.
# Agents: run after gates pass; user rule — do not ask human to push manually.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/li-ui.sh
source "$ROOT/scripts/lib/li-ui.sh"
cd "$ROOT"
WRAPPER="$ROOT/scripts/with-github-env.sh"

branch="$(git rev-parse --abbrev-ref HEAD)"
msg="${1:-chore: agent sync $(date -u +%Y-%m-%dT%H:%MZ)}"

if [[ "${LI_SKIP_AUTO_PUSH:-}" == "1" ]]; then
  echo "agent-push-github: skipped (LI_SKIP_AUTO_PUSH=1)"
  exit 0
fi

if ! git diff --quiet 2>/dev/null || ! git diff --cached --quiet 2>/dev/null ||
  [[ -n "$(git ls-files --others --exclude-standard)" ]]; then
  git add -A
  if git diff --cached --quiet; then
    echo "agent-push-github: nothing to commit"
  else
    git commit -m "$msg"
  fi
else
  echo "agent-push-github: working tree clean"
fi

"$WRAPPER" gh auth setup-git 2>/dev/null || true

if git remote get-url origin &>/dev/null; then
  li_phase "push origin $branch"
  "$WRAPPER" git push -u origin "$branch"
fi

if git remote get-url langverse &>/dev/null; then
  li_phase "push langverse → main"
  "$WRAPPER" git push langverse "${branch}:main"
fi

li_ok "agent-push-github"
