#!/usr/bin/env bash
# Gate: compiler ↔ downstream-package isolation.
#
# The compiler (compiler/, runtime/, bootstrap/) is the upstream toolchain: it
# must never depend on, embed, or hardcode the downstream package tree
# (packages/). Packages are user code compiled BY lic, never linked INTO it.
# This gate fails if the carve-out is breached in either direction:
#
#   1. compiler/runtime/bootstrap source must not reference packages/ paths.
#   2. compiler build files (CMake) must not add package dirs or link package
#      targets.
#   3. std/ facades (the compiler's own prelude) must not import packages.
#   4. Packages must be top-level (packages/<name>/li.toml) — no nesting, no
#      sub-packages, no package living inside another package's tree.
#   5. Every [workspace].members package must declare [dependencies] with
#      `path = "../<name>"` only (sibling refs inside packages/), never
#      absolute or compiler-internal paths.
#
# CI: scripts/ci.sh "compiler isolation (carve-out)".
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

fail=0
note_fail() {
  echo "check-compiler-isolation: FAIL — $1" >&2
  fail=1
}

# --- 1. compiler/runtime/bootstrap source must not reference packages/ ------
# Only non-comment lines count: documentation comments may name a downstream
# package for context, but code must never read/write/compile packages/.
for dir in compiler runtime bootstrap; do
  hits=$(grep -rn "packages/" "$ROOT/$dir" \
    --include='*.cpp' --include='*.hpp' --include='*.h' --include='*.c' \
    --include='*.cmake' --include='CMakeLists.txt' 2>/dev/null \
    | grep -vE '^[^:]*:[0-9]+:\s*(/\*|\*|//)' || true)
  if [[ -n "$hits" ]]; then
    note_fail "$dir/ references packages/ (source must stay upstream-only):"
    echo "$hits" | sed 's/^/    /' >&2
  fi
 done

# --- 2. CMake must not add package dirs or link package targets -------------
cmake_hits=$(grep -rn "packages" "$ROOT/compiler/CMakeLists.txt" \
  "$ROOT/compiler"/*/CMakeLists.txt "$ROOT/runtime/CMakeLists.txt" 2>/dev/null \
  | grep -v "add_subdirectory(compiler)" || true)
if [[ -n "$cmake_hits" ]]; then
  note_fail "CMake references packages/ (compiler build must be self-contained):"
  echo "$cmake_hits" | sed 's/^/    /' >&2
fi

# --- 3. std/ facades must not import downstream packages --------------------
std_imports=$(grep -rn '^import ' "$ROOT/std" --include='*.li' 2>/dev/null \
  | grep -vE 'import (std|core|prelude)\.' || true)
if [[ -n "$std_imports" ]]; then
  note_fail "std/ imports a non-std module (prelude must stay package-free):"
  echo "$std_imports" | sed 's/^/    /' >&2
fi

# --- 4. packages must be top-level, never nested ----------------------------
# A package's own li.toml must sit directly under packages/<name>/li.toml.
nested=$(find "$ROOT/packages" -mindepth 3 -maxdepth 4 -name 'li.toml' \
  -not -path '*/li-tests/*' 2>/dev/null | grep -vE '^[^ ]*/packages/[^/]+/li\.toml$' || true)
if [[ -n "$nested" ]]; then
  note_fail "nested package found (every package is top-level packages/<name>/):"
  echo "$nested" | sed 's/^/    /' >&2
fi

# --- 5. package dependencies must be sibling path refs only ------------------
# Scope to the [dependencies] section only ([[bin]] entries legitimately use
# non-sibling paths like "src/main.li").
bad_dep=$(for f in "$ROOT"/packages/*/li.toml; do
  awk '/^\[dependencies\]/{in_dep=1; next} /^\[/{in_dep=0} in_dep && /path[[:space:]]*=/{print FILENAME ":" NR ":" $0}' "$f"
done | grep -vE 'path[[:space:]]*=[[:space:]]*"\.\./[a-z0-9-]+"' || true)
if [[ -n "$bad_dep" ]]; then
  note_fail "package dependency escapes packages/ (must be path = \"../<name>\"):"
  echo "$bad_dep" | sed 's/^/    /' >&2
fi

if [[ "$fail" -eq 0 ]]; then
  echo "check-compiler-isolation: ok (compiler upstream-only; packages top-level, sibling deps only)"
  exit 0
fi
exit 1
