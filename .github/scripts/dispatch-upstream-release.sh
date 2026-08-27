#!/usr/bin/env bash
set -euo pipefail
EVENT="${1:?lic-release|lit-release|lip-release}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIST="$ROOT/.github/li-downstream-repos.txt"
VERSION="${VERSION:-${GITHUB_REF_NAME:-}}"
[[ -z "$VERSION" ]] && VERSION="${GITHUB_EVENT_RELEASE_TAG_NAME:-}"
[[ -z "$VERSION" ]] && VERSION="${INPUT_VERSION:-}"
[[ -z "$VERSION" ]] && { echo "VERSION not set" >&2; exit 1; }
case "$EVENT" in
  lic-release) SRC="li-langverse/lic" ;;
  lit-release) SRC="li-langverse/lit" ;;
  lip-release) SRC="li-langverse/lip" ;;
  *) exit 1 ;;
esac
command -v gh >/dev/null || { echo "gh required" >&2; exit 1; }
OK=0 FAIL=0
while IFS= read -r repo || [[ -n "$repo" ]]; do
  repo="${repo%%#*}"; repo="$(echo "$repo" | tr -d '[:space:]')"
  [[ -z "$repo" ]] && continue
  if gh api "repos/${repo}/dispatches" -f "event_type=${EVENT}" \
    -f "client_payload[version]=${VERSION}" -f "client_payload[repo]=${SRC}" 2>/dev/null; then
    echo "dispatched ${EVENT} -> ${repo} (${VERSION})"; OK=$((OK+1))
  else
    echo "FAIL -> ${repo}" >&2; FAIL=$((FAIL+1))
  fi
done < "$LIST"
echo "ok=${OK} fail=${FAIL}"; [[ "$FAIL" -eq 0 ]]
