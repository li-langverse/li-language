#!/usr/bin/env bash
# Validate VERSION, CHANGELOG, and (optionally) lic --version alignment.
#
# Usage:
#   ./scripts/check-version.sh           # format + changelog section
#   ./scripts/check-version.sh --pr      # also enforce bump rules on PRs
#   ./scripts/check-version.sh --build # also run lic --version after build
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_FILE="${ROOT}/VERSION"
CHANGELOG="${ROOT}/CHANGELOG.md"
SEMVER_RE='^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?(\+[0-9A-Za-z.-]+)?$'

MODE_PR=0
MODE_BUILD=0
for arg in "$@"; do
  case "$arg" in
    --pr) MODE_PR=1 ;;
    --build) MODE_BUILD=1 ;;
    -h|--help)
      sed -n '2,8p' "$0"
      exit 0
      ;;
    *)
      echo "check-version: unknown arg: $arg" >&2
      exit 2
      ;;
  esac
done

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "check-version: missing $VERSION_FILE" >&2
  exit 1
fi

VERSION="$(tr -d '[:space:]' < "$VERSION_FILE")"
if [[ ! "$VERSION" =~ $SEMVER_RE ]]; then
  echo "check-version: VERSION '$VERSION' is not valid semver" >&2
  exit 1
fi

if [[ ! -f "$CHANGELOG" ]]; then
  echo "check-version: missing $CHANGELOG" >&2
  exit 1
fi

if ! grep -qE "^## \\[${VERSION}\\]" "$CHANGELOG"; then
  echo "check-version: CHANGELOG.md must contain a '## [${VERSION}]' section" >&2
  exit 1
fi

base_version() {
  local ref="$1"
  git show "${ref}:VERSION" 2>/dev/null | tr -d '[:space:]' || true
}

if [[ "$MODE_PR" -eq 1 ]]; then
  BASE_REF="${GITHUB_BASE_REF:-main}"
  if git rev-parse "origin/${BASE_REF}" >/dev/null 2>&1; then
    MERGE_BASE="$(git merge-base "origin/${BASE_REF}" HEAD)"
    OLD="$(base_version "$MERGE_BASE")"
    if [[ -n "$OLD" && "$OLD" != "$VERSION" ]]; then
      echo "check-version: VERSION bump ${OLD} -> ${VERSION}"
      if ! grep -qE "^## \\[${VERSION}\\]" "$CHANGELOG"; then
        echo "check-version: bump requires CHANGELOG section for ${VERSION}" >&2
        exit 1
      fi
      # Downgrades are not allowed on PRs.
      if [[ "$(printf '%s\n%s\n' "$OLD" "$VERSION" | sort -V | head -1)" != "$OLD" ]]; then
        echo "check-version: VERSION must not decrease (${OLD} -> ${VERSION})" >&2
        exit 1
      fi
    elif [[ -z "$OLD" ]]; then
      echo "check-version: no base VERSION at ${MERGE_BASE}; skipping bump rules"
    else
      echo "check-version: VERSION unchanged (${VERSION})"
    fi
  else
    echo "check-version: origin/${BASE_REF} not found; skipping PR bump rules"
  fi
fi

if [[ "$MODE_BUILD" -eq 1 ]]; then
  LIC="${ROOT}/build/compiler/lic/lic"
  if [[ ! -x "$LIC" ]]; then
    echo "check-version: lic not built; run ./scripts/build.sh first" >&2
    exit 1
  fi
  OUT="$("$LIC" --version)"
  EXPECTED="lic ${VERSION}"
  if [[ "$OUT" != "$EXPECTED" ]]; then
    echo "check-version: expected '$EXPECTED', got '$OUT'" >&2
    exit 1
  fi
  echo "check-version: lic --version ok ($OUT)"
fi

echo "check-version: ok (${VERSION})"
