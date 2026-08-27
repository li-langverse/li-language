#!/usr/bin/env bash
# Lightweight traceability checks for official packages (Pkg phase).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OFFICIAL="$ROOT/docs/ecosystem/official-packages.md"
FAIL=0

check_file() {
  local f="$1"
  if [[ ! -f "$f" ]]; then
    echo "FAIL missing: $f"
    FAIL=1
  fi
}

if [[ ! -f "$OFFICIAL" ]]; then
  echo "FAIL missing $OFFICIAL"
  exit 1
fi

if ! grep -q 'li-langverse' "$OFFICIAL"; then
  echo "FAIL $OFFICIAL: must reference li-langverse org"
  FAIL=1
fi

if [[ -d "$ROOT/packages" ]]; then
  for pub in "$ROOT/packages"/*/PUBLISH.md; do
    [[ -f "$pub" ]] || continue
    if ! grep -qE 'PKG-[a-zA-Z0-9_-]+' "$pub"; then
      echo "FAIL $pub: missing PKG- id"
      FAIL=1
    fi
    check_file "$(dirname "$pub")/CHANGELOG.md"
    check_file "$(dirname "$pub")/SECURITY.md"
    ci="$(dirname "$pub")/.github/workflows/ci.yml"
    if [[ ! -f "$ci" ]]; then
      echo "FAIL $ci: missing (run ./scripts/ensure-package-ci.sh)"
      FAIL=1
    fi
  done
fi

if [[ "$FAIL" -ne 0 ]]; then
  exit 1
fi
echo "check-traceability: ok"
