#!/usr/bin/env bash
# Export org/repo metadata JSON for disaster recovery (no secret values).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WRAPPER="$ROOT/scripts/with-github-backup-env.sh"

LIVE_ORG="${LIVE_ORG:-li-langverse}"
BACKUP_ROOT="${BACKUP_ROOT:-$HOME/Documents/li-langverse-backup}"
STAMP="$(date -u +%Y-%m-%dT%H%M%SZ)"
OUT="$BACKUP_ROOT/metadata/$STAMP"
mkdir -p "$OUT"

echo "==> export org ${LIVE_ORG} -> $OUT"
"$WRAPPER" gh api "orgs/${LIVE_ORG}" >"$OUT/org.json"
"$WRAPPER" gh api "orgs/${LIVE_ORG}/repos" --paginate >"$OUT/repos.json"

REPOS=()
while IFS= read -r _repo; do
  [[ -n "$_repo" ]] && REPOS+=("$_repo")
done < <("$WRAPPER" gh api "orgs/${LIVE_ORG}/repos" --paginate -q '.[].name')
for name in "${REPOS[@]}"; do
  repo="${LIVE_ORG}/${name}"
  safe="${name}"
  echo "==> $repo"
  "$WRAPPER" gh api "repos/${repo}" >"$OUT/repo-${safe}-meta.json" 2>/dev/null || true
  "$WRAPPER" gh api "repos/${repo}/branches" --paginate >"$OUT/repo-${safe}-branches.json" 2>/dev/null || true
  "$WRAPPER" gh api "repos/${repo}/rulesets" >"$OUT/repo-${safe}-rulesets.json" 2>/dev/null || true
  "$WRAPPER" gh api "repos/${repo}/hooks" >"$OUT/repo-${safe}-hooks.json" 2>/dev/null || true
  "$WRAPPER" gh api "repos/${repo}/actions/secrets" >"$OUT/repo-${safe}-secret-names.json" 2>/dev/null || true
  "$WRAPPER" gh api "repos/${repo}/collaborators" >"$OUT/repo-${safe}-collaborators.json" 2>/dev/null || true
done

echo "export-li-langverse-metadata: ok — $OUT"
