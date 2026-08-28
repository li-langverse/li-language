#!/usr/bin/env bash
# CWE-78/88: lic build must reject shell metacharacters in -o and LI_EXTRA_C paths.
# Runs on Linux, macOS, and Windows (Git Bash) — same cases everywhere we can.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIC="$("$ROOT/scripts/resolve-lic.sh")"
SRC="$ROOT/li-tests/lexer_parser/fib.li"
FAIL=0
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
# Normalize for Windows Git Bash (backslashes in TEMP break quoted clang args).
TMP_BASE="${TMP_BASE//\\//}"

if [[ ! -x "$LIC" ]]; then
  echo "codegen_path_injection: skip (no lic)"
  exit 0
fi

try_build() {
  local label="$1"
  shift
  local err rc
  err="$("$@" 2>&1)" && rc=0 || rc=$?
  if [[ "$rc" -eq 0 ]]; then
    echo "FAIL $label (build succeeded — expected rejection)"
    FAIL=1
    return
  fi
  if echo "$err" | grep -qiE 'unsafe characters|build failed'; then
    echo "PASS $label"
  else
    echo "PASS $label (non-zero exit $rc)"
  fi
}

try_build "output path semicolon" \
  env -u LI_EXTRA_C "$LIC" build "$SRC" -o "${TMP_BASE}/li-evil;id"

try_build "output path subshell" \
  env -u LI_EXTRA_C "$LIC" build "$SRC" -o "${TMP_BASE}/li-\$(id)"

try_build "LI_EXTRA_C pipe" \
  env LI_EXTRA_C="${TMP_BASE}/x|id" "$LIC" build "$SRC" -o "${TMP_BASE}/li-out-$$.bin"

if [[ "$(uname -s 2>/dev/null)" == MINGW* ]] || [[ "$(uname -s 2>/dev/null)" == MSYS* ]] ||
  [[ -n "${WINDIR:-}" ]]; then
  try_build "output path percent (Windows cmd)" \
    env -u LI_EXTRA_C "$LIC" build "$SRC" -o "${TMP_BASE}/li-%OS%.bin"
fi

echo "--- codegen_path_injection: fail=$FAIL ($(uname -s 2>/dev/null || echo unknown))"
[[ "$FAIL" -eq 0 ]]
