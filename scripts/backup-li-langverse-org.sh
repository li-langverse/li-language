#!/usr/bin/env bash
# Mirror all repos from LIVE_ORG to local bare clones and li-langverse-backup (private).
# Human-operated only — uses .env.github.backup, not agent .env.github.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WRAPPER="$ROOT/scripts/with-github-backup-env.sh"

LIVE_ORG="${LIVE_ORG:-li-langverse}"
BACKUP_OWNER="${BACKUP_OWNER:-li-langverse-backup}"
BACKUP_ROOT="${BACKUP_ROOT:-$HOME/Documents/li-langverse-backup}"
BACKUP_PUSH="${BACKUP_PUSH:-1}"

if [[ "$BACKUP_OWNER" == "$LIVE_ORG" ]]; then
  echo "backup-li-langverse-org: BACKUP_OWNER must not equal LIVE_ORG ($LIVE_ORG)" >&2
  exit 1
fi

"$WRAPPER" gh auth setup-git 2>/dev/null || true

MIRROR_BASE="$BACKUP_ROOT/mirrors/$LIVE_ORG"
mkdir -p "$MIRROR_BASE"
STAMP="$(date -u +%Y-%m-%dT%H%M%SZ)"
REPORT="$BACKUP_ROOT/LAST_SUCCESS.txt"
TMP_REPORT="$(mktemp)"
{
  echo "# li-langverse org backup — $STAMP"
  echo "live_org=$LIVE_ORG backup_owner=$BACKUP_OWNER"
} >"$TMP_REPORT"

ensure_backup_repo() {
  local name="$1"
  if "$WRAPPER" gh repo view "${BACKUP_OWNER}/${name}" &>/dev/null; then
    return 0
  fi
  echo "==> create ${BACKUP_OWNER}/${name} (private mirror)"
  if ! "$WRAPPER" gh repo create "${BACKUP_OWNER}/${name}" --private \
    --description "mirror of ${LIVE_ORG}/${name}" 2>/dev/null; then
    "$WRAPPER" gh repo view "${BACKUP_OWNER}/${name}" &>/dev/null
  fi
}

strip_pull_refs() {
  local mirror="$1"
  while IFS= read -r ref; do
    [[ -n "$ref" ]] && git -C "$mirror" update-ref -d "$ref" 2>/dev/null || true
  done < <(git -C "$mirror" for-each-ref --format='%(refname)' refs/pull 2>/dev/null || true)
}

REPOS=()
while IFS= read -r _repo; do
  [[ -n "$_repo" ]] && REPOS+=("$_repo")
done < <("$WRAPPER" gh api "orgs/${LIVE_ORG}/repos" --paginate -q '.[].name')
if [[ ${#REPOS[@]} -eq 0 ]]; then
  echo "backup-li-langverse-org: no repos in org ${LIVE_ORG}" >&2
  exit 1
fi

FAIL=0
for name in "${REPOS[@]}"; do
  mirror="$MIRROR_BASE/${name}.git"
  echo "==> mirror ${LIVE_ORG}/${name}"
  if [[ -d "$mirror" ]]; then
    if ! git -C "$mirror" remote update --prune; then
      echo "backup-li-langverse-org: remote update failed for $name" >&2
      FAIL=1
      continue
    fi
  else
    mkdir -p "$(dirname "$mirror")"
    if ! git clone --mirror "https://github.com/${LIVE_ORG}/${name}.git" "$mirror"; then
      echo "backup-li-langverse-org: clone failed for $name" >&2
      FAIL=1
      continue
    fi
  fi

  head_sha="$(git -C "$mirror" rev-parse HEAD 2>/dev/null || echo unknown)"

  if [[ "$BACKUP_PUSH" == "1" ]]; then
    ensure_backup_repo "$name"
    strip_pull_refs "$mirror"
    backup_url="https://github.com/${BACKUP_OWNER}/${name}.git"
    if ! "$WRAPPER" git -C "$mirror" push --force --prune "$backup_url" \
      'refs/heads/*:refs/heads/*' 'refs/tags/*:refs/tags/*'; then
      echo "backup-li-langverse-org: push to backup failed for $name" >&2
      FAIL=1
      continue
    fi
    remote_ts="$("$WRAPPER" gh api "repos/${BACKUP_OWNER}/${name}" -q '.pushed_at' 2>/dev/null || echo ok)"
    echo "${LIVE_ORG}/${name} local_head=${head_sha} backup_pushed_at=${remote_ts}" >>"$TMP_REPORT"
  else
    echo "${LIVE_ORG}/${name} local_head=${head_sha} backup_push=skipped" >>"$TMP_REPORT"
  fi
done

if [[ "$FAIL" -ne 0 ]]; then
  rm -f "$TMP_REPORT"
  exit 1
fi

mv "$TMP_REPORT" "$REPORT"
echo "backup-li-langverse-org: ok — report $REPORT"
