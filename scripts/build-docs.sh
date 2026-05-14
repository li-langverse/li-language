#!/usr/bin/env bash
# Build the MkDocs site to ./site (used locally and in CI).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENV="${ROOT}/.venv-docs"

if [[ ! -d "$VENV" ]]; then
  python3 -m venv "$VENV"
fi
# shellcheck disable=SC1091
source "$VENV/bin/activate"
pip install -q -r "${ROOT}/docs/requirements.txt"

mkdocs build -f "${ROOT}/mkdocs.yml" -d "${ROOT}/site" "$@"
echo "docs: wrote ${ROOT}/site"
