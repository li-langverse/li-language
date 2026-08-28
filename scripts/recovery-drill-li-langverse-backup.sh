#!/usr/bin/env bash
# Dry-run: restore one repo from local bare mirror to a throwaway repo on BACKUP_OWNER, then delete it.
# Human-operated only — uses .env.github.backup.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WRAPPER="$ROOT/scripts/with-github-backup-env.sh"

LIVE_ORG="${LIVE_ORG:-li-langverse}"
BACKUP_OWNER="${BACKUP_OWNER:-li-langverse-backup}"
BACKUP_ROOT="${BACKUP_ROOT:-$HOME/Documents/li-langverse-backup}"
DRILL_REPO="${DRILL_REPO:-lic-recovery-drill}"
SOURCE_REPO="${SOURCE_REPO:-lic}"

mirror="$BACKUP_ROOT/mirrors/$LIVE_ORG/${SOURCE_REPO}.git"
if [[ ! -d "$mirror" ]]; then
  echo "recovery-drill: missing mirror $mirror — run backup-li-langverse-org.sh first" >&2
  exit 1
fi

"$WRAPPER" gh auth setup-git 2>/dev/null || true

if "$WRAPPER" gh repo view "${BACKUP_OWNER}/${DRILL_REPO}" &>/dev/null; then
  echo "==> delete stale ${BACKUP_OWNER}/${DRILL_REPO}"
  "$WRAPPER" gh repo delete "${BACKUP_OWNER}/${DRILL_REPO}" --yes
fi

echo "==> create ${BACKUP_OWNER}/${DRILL_REPO}"
"$WRAPPER" gh repo create "${BACKUP_OWNER}/${DRILL_REPO}" --private \
  --description "recovery drill (delete after verify)"

backup_url="https://github.com/${BACKUP_OWNER}/${DRILL_REPO}.git"
if ! "$WRAPPER" git -C "$mirror" push --force "$backup_url" \
  'refs/heads/*:refs/heads/*' 'refs/tags/*:refs/tags/*'; then
  echo "recovery-drill: push failed" >&2
  "$WRAPPER" gh repo delete "${BACKUP_OWNER}/${DRILL_REPO}" --yes 2>/dev/null || true
  exit 1
fi

local_main="$(git -C "$mirror" rev-parse refs/heads/main 2>/dev/null || true)"
remote_main="$("$WRAPPER" gh api "repos/${BACKUP_OWNER}/${DRILL_REPO}/commits/main" -q '.sha' 2>/dev/null || true)"
if [[ -z "$local_main" || "$local_main" != "$remote_main" ]]; then
  echo "recovery-drill: main SHA mismatch local=$local_main remote=$remote_main" >&2
  "$WRAPPER" gh repo delete "${BACKUP_OWNER}/${DRILL_REPO}" --yes 2>/dev/null || true
  exit 1
fi

echo "recovery-drill: verified main=$local_main"
echo "==> delete ${BACKUP_OWNER}/${DRILL_REPO}"
"$WRAPPER" gh repo delete "${BACKUP_OWNER}/${DRILL_REPO}" --yes
echo "recovery-drill: ok"
