#!/usr/bin/env bash
# Cursor stop — warn if working tree diff may weaken proof/security/perf gates.
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel 2>/dev/null)" || exit 0
cd "$repo_root"

diff_unified() {
  {
    git diff --unified=0 HEAD 2>/dev/null || true
    git diff --unified=0 --cached HEAD 2>/dev/null || true
  } | sed '/^+++/d' | sed '/^---/d'
}

patterns=(
  'sorry'
  'assume[[:space:]]'
  '\bunsafe\b'
  'gates\.proof[[:space:]]*=[[:space:]]*false'
  'proof\.lean[[:space:]]*=[[:space:]]*"off"'
  'LI_ALLOW_OPEN_VC=1'
  'LI_BUILD_VERIFY_LEAN=0'
  '--no-verify'
  'check_stdlib_seal'
)

found=()
body="$(diff_unified)"
[[ -n "$body" ]] || exit 0

for pat in "${patterns[@]}"; do
  if echo "$body" | grep -qiE "$pat" 2>/dev/null; then
    found+=("$pat")
  fi
done

# Deleted stdlib_seal tests
if git diff --name-only --diff-filter=D HEAD 2>/dev/null | grep -q 'li-tests/stdlib_seal/' || \
   git diff --name-only --cached --diff-filter=D HEAD 2>/dev/null | grep -q 'li-tests/stdlib_seal/'; then
  found+=('deleted li-tests/stdlib_seal/')
fi

if ((${#found[@]} > 0)); then
  echo "strict-by-default guard: diff may weaken gates (review before push): ${found[*]}" >&2
  echo "  policy: docs/ecosystem/strict-by-default.md — explicit li.toml [gates] or documented env only" >&2
fi

exit 0
