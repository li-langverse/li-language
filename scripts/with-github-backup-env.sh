#!/usr/bin/env bash
# Load ../.env.github.backup and run a command (backup only — not for agents).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV="${LI_BACKUP_ENV:-$(cd "$ROOT/.." && pwd)/.env.github.backup}"
if [[ ! -f "$ENV" ]]; then
  echo "with-github-backup-env: missing $ENV — copy scripts/env.github.backup.example" >&2
  exit 1
fi
set -a
# shellcheck source=/dev/null
source "$ENV"
set +a
export GH_TOKEN="${BACKUP_GH_TOKEN:-${GH_TOKEN:-}}"
if [[ -z "${GH_TOKEN:-}" ]]; then
  echo "with-github-backup-env: set BACKUP_GH_TOKEN or GH_TOKEN in $ENV" >&2
  exit 1
fi
exec "$@"
