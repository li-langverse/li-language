#!/usr/bin/env bash
set -euo pipefail
LI_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WRAPPER="$LI_ROOT/scripts/with-github-env.sh"
LIP_ROOT="$(cd "$LI_ROOT/../lip" && pwd)"
LIT_ROOT="$(cd "$LI_ROOT/../lit" && pwd)"
ROADMAP_ROOT="$(cd "$LI_ROOT/../roadmap" 2>/dev/null && pwd)" || ROADMAP_ROOT=""
BENCH_ROOT="$(cd "$LI_ROOT/../benchmarks" 2>/dev/null && pwd)" || BENCH_ROOT=""

setup_https_push() {
  local dir="$1" repo="$2" remote="${3:-origin}"
  "$WRAPPER" gh auth setup-git 2>/dev/null || true
  git -C "$dir" remote set-url "$remote" "https://github.com/${repo}.git"
}

echo "==> lip"
setup_https_push "$LIP_ROOT" "li-langverse/lip"
"$WRAPPER" git -C "$LIP_ROOT" push -u origin main

echo "==> lit"
setup_https_push "$LIT_ROOT" "li-langverse/lit"
"$WRAPPER" git -C "$LIT_ROOT" push -u origin main

if [[ -n "$ROADMAP_ROOT" && -d "$ROADMAP_ROOT/.git" ]]; then
  echo "==> roadmap"
  setup_https_push "$ROADMAP_ROOT" "li-langverse/roadmap"
  "$WRAPPER" git -C "$ROADMAP_ROOT" push -u origin main
fi

if [[ -n "$BENCH_ROOT" && -d "$BENCH_ROOT/.git" ]]; then
  echo "==> benchmarks"
  setup_https_push "$BENCH_ROOT" "li-langverse/benchmarks"
  "$WRAPPER" git -C "$BENCH_ROOT" push -u origin main
fi

echo "==> lic (dev -> main on langverse; origin unchanged)"
git -C "$LI_ROOT" remote get-url langverse &>/dev/null || \
  git -C "$LI_ROOT" remote add langverse https://github.com/li-langverse/lic.git
setup_https_push "$LI_ROOT" "li-langverse/lic" langverse
"$WRAPPER" git -C "$LI_ROOT" push langverse dev:main

echo "Done. Cloud Actions: set LI_DOWNSTREAM_DISPATCH_TOKEN on li-langverse/lic separately."
