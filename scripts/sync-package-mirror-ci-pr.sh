#!/usr/bin/env bash
# Open a PR on li-langverse/<NAME> to sync .github/workflows/ci.yml from monorepo template.
# Usage: ./scripts/sync-package-mirror-ci-pr.sh NAME
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NAME="${1:?usage: sync-package-mirror-ci-pr.sh NAME}"
ORG="${LI_ORG:-li-langverse}"
REPO="${ORG}/${NAME}"
TPL="$ROOT/scripts/templates/github-repo/ci.yml"
BRANCH="ci/pin-lic-main"
PATH_WF=".github/workflows/ci.yml"

command -v gh >/dev/null || { echo "gh required" >&2; exit 1; }
[[ -f "$TPL" ]] || { echo "missing $TPL" >&2; exit 1; }

SHA_MAIN="$(gh api "repos/${REPO}/git/ref/heads/main" -q .object.sha)"
gh api "repos/${REPO}/git/refs" -f ref="refs/heads/${BRANCH}" -f sha="$SHA_MAIN" 2>/dev/null \
  || gh api -X PATCH "repos/${REPO}/git/refs/heads/${BRANCH}" -f sha="$SHA_MAIN"

FILE_SHA="$(gh api "repos/${REPO}/contents/${PATH_WF}?ref=${BRANCH}" -q .sha 2>/dev/null || true)"
B64="$(base64 -w0 "$TPL" 2>/dev/null || base64 "$TPL" | tr -d '\n')"
BODY="$(jq -n --arg c "$B64" --arg b "$BRANCH" --arg fs "${FILE_SHA:-}" \
  '{message:"ci: sync workflow from lic template (lic@main)", content:$c, branch:$b}
   + (if $fs != "" then {sha:$fs} else {} end)')"
gh api -X PUT "repos/${REPO}/contents/${PATH_WF}" --input <(echo "$BODY") >/dev/null

EXIST="$(gh pr list --repo "$REPO" --head "$BRANCH" --json number -q '.[0].number' 2>/dev/null || true)"
if [[ -n "$EXIST" && "$EXIST" != "null" ]]; then
  echo "PR already open: https://github.com/${REPO}/pull/${EXIST}"
  exit 0
fi

gh pr create --repo "$REPO" --base main --head "$BRANCH" \
  --title "ci: pin lic@main" \
  --body "Sync \`${PATH_WF}\` from [\`lic\`](https://github.com/li-langverse/lic) monorepo template. Required branch protection context: **check**."
echo "Opened PR on ${REPO}"
