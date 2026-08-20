#!/usr/bin/env bash
# Fail if any .li file uses proc (def-only Li; trusted FFI is extern def in seam.li).
set -euo pipefail

ROOT="${1:-.}"

# Negative compile tests intentionally contain bare `proc` / `extern proc`; compiler suite covers them.
should_skip() {
  case "$1" in
    *"/li-tests/"*) return 0 ;;
  esac
  return 1
}

bad=0

file_path_from_rg_hit() {
  local hit="$1"
  if [[ "$hit" =~ ^(.+):[0-9]+: ]]; then
    printf '%s\n' "${BASH_REMATCH[1]}"
  else
    printf '%s\n' "${hit%%:*}"
  fi
}

scan_file() {
  local f="$1"
  local line
  while IFS= read -r line || [[ -n "$line" ]]; do
    if [[ "$line" =~ ^[[:space:]]*proc[[:space:]] ]]; then
      echo "check-li-def-syntax: ${f}:${n}: $line"
      bad=1
    elif [[ "$line" =~ async[[:space:]]+proc[[:space:]] ]]; then
      echo "check-li-def-syntax: ${f}:${n}: $line (use async def)"
      bad=1
    elif [[ "$line" =~ ^[[:space:]]*extern[[:space:]]+proc[[:space:]] ]]; then
      echo "check-li-def-syntax: ${f}:${n}: $line (use extern def)"
      bad=1
    fi
  done < <(grep -n '' "$f" 2>/dev/null || true)
}

if command -v rg >/dev/null 2>&1; then
  while IFS= read -r hit; do
    [[ -z "$hit" ]] && continue
    should_skip "$(file_path_from_rg_hit "$hit")" && continue
    echo "check-li-def-syntax: $hit"
    bad=1
  done < <(rg -n '^[[:space:]]*proc\b' --glob '*.li' "$ROOT" 2>/dev/null || true)
  while IFS= read -r hit; do
    [[ -z "$hit" ]] && continue
    should_skip "$(file_path_from_rg_hit "$hit")" && continue
    echo "check-li-def-syntax: $hit (use async def)"
    bad=1
  done < <(rg -n '\basync proc\b' --glob '*.li' "$ROOT" 2>/dev/null || true)
  while IFS= read -r hit; do
    [[ -z "$hit" ]] && continue
    should_skip "$(file_path_from_rg_hit "$hit")" && continue
    echo "check-li-def-syntax: $hit (use extern def)"
    bad=1
  done < <(rg -n '^[[:space:]]*extern[[:space:]]+proc\b' --glob '*.li' "$ROOT" 2>/dev/null || true)
else
  while IFS= read -r -d '' f; do
    should_skip "$f" && continue
    scan_file "$f"
  done < <(find "$ROOT" -name '*.li' -type f -print0 2>/dev/null)
fi

if [[ "$bad" -ne 0 ]]; then
  echo "check-li-def-syntax: use 'def' for Li procedures; trusted FFI is 'extern def'" >&2
  exit 1
fi

echo "check-li-def-syntax: ok under $ROOT"
