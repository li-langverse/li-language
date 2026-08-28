#!/usr/bin/env bash
# Open PRs on official package mirrors: sync sources + CI from lic/packages/<folder>.
# Usage: ./scripts/sync-package-mirror-def-syntax-pr.sh REPO [REPO...]
#   REPO = GitHub repo name (e.g. li-std-core); monorepo folder resolved via map below.
# Requires: gh auth, network.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ORG="${LI_ORG:-li-langverse}"
BRANCH="${LI_SYNC_BRANCH:-cursor/sync-lic-main-57b4}"

# GitHub mirror repo → packages/ folder (after import-based renames on lic main).
package_folder_for_repo() {
  case "$1" in
    li-std-core) echo "li-core" ;;
    li-std-math) echo "li-math" ;;
    li-httpd) echo "li-net-httpd" ;;
    li-std-numerics) echo "li-math-numerics" ;;
    *) echo "$1" ;;
  esac
}

if [[ $# -lt 1 ]]; then
  echo "usage: $0 REPO [REPO...]" >&2
  echo "  e.g. $0 li-std-core li-std-math li-httpd li-net li-demo" >&2
  exit 1
fi

command -v gh >/dev/null || { echo "gh required" >&2; exit 1; }

sync_tree() {
  local src="$1" dst="$2"
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --exclude='.git' "$src/" "$dst/"
  else
    cp -a "$src/." "$dst/"
  fi
}

push_repo() {
  local repo_name="$1"
  local pkg_name
  pkg_name="$(package_folder_for_repo "$repo_name")"
  local pkg="$ROOT/packages/$pkg_name"
  local repo="${ORG}/${repo_name}"
  if [[ ! -d "$pkg" ]]; then
    echo "skip $repo_name (no packages/$pkg_name)" >&2
    return 0
  fi
  if ! gh repo view "$repo" >/dev/null 2>&1; then
    echo "skip $repo_name (no github repo $repo)" >&2
    return 0
  fi

  local tmp
  tmp="$(mktemp -d)"

  gh repo clone "$repo" "$tmp/repo" -- --depth 1 -b main
  cd "$tmp/repo"
  sync_tree "$pkg" .
  git checkout -B "$BRANCH"
  git add -A
  if git diff --cached --quiet; then
    echo "$repo_name: no changes vs mirror"
    cd "$ROOT"
    rm -rf "$tmp"
    return 0
  fi
  git -c user.name="li-langverse-bot" -c user.email="bot@users.noreply.github.com" \
    commit -m "chore: sync packages/${pkg_name} from lic main (def syntax)"

  local remote="https://github.com/${repo}.git"
  if [[ -n "${GH_TOKEN:-}" ]]; then
    remote="https://x-access-token:${GH_TOKEN}@github.com/${repo}.git"
  elif command -v gh >/dev/null 2>&1; then
    remote="https://x-access-token:$(gh auth token)@github.com/${repo}.git"
  fi
  git push "$remote" "$BRANCH" --force-with-lease 2>/dev/null \
    || git push "$remote" "$BRANCH" --force

  local exist
  exist="$(gh pr list --repo "$repo" --head "$BRANCH" --json number -q '.[0].number' 2>/dev/null || true)"
  if [[ -n "$exist" && "$exist" != "null" ]]; then
    echo "$repo_name: PR https://github.com/${repo}/pull/${exist}"
  else
    gh pr create --repo "$repo" --base main --head "$BRANCH" \
      --title "chore: sync from lic main (def syntax, comment cleanup)" \
      --body "Sync \`packages/${pkg_name}/\` from [lic@main](https://github.com/li-langverse/lic) after https://github.com/li-langverse/lic/pull/58 — \`def\` for procedures, \`extern proc\` for FFI, stripped agent/history header comments.

Mirror repo \`${repo_name}\` ↔ monorepo \`packages/${pkg_name}/\`."
    echo "$repo_name: opened PR"
  fi
  cd "$ROOT"
  rm -rf "$tmp"
}

for name in "$@"; do
  push_repo "$name"
done
