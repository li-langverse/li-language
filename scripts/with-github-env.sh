#!/usr/bin/env bash
# Load ../.env.github and run a command (for agent / local gh git).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV="${LI_GITHUB_ENV:-$(cd "$ROOT/.." && pwd)/.env.github}"
if [[ ! -f "$ENV" ]]; then
  echo "with-github-env: missing $ENV — copy .env.github.example and set GH_TOKEN=" >&2
  exit 1
fi
if [[ -z "${GH_TOKEN:-}" ]] && grep -qE '^GH_TOKEN=$' "$ENV" 2>/dev/null; then
  echo "with-github-env: edit $ENV and set GH_TOKEN (fine-grained PAT)" >&2
  exit 1
fi
set -a
# shellcheck source=/dev/null
source "$ENV"
set +a
export GH_TOKEN
exec "$@"
