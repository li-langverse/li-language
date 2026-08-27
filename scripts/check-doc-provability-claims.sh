#!/usr/bin/env bash
# Fail if handbook pages overclaim Lean / proof certificate without linking gaps doc.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAPS="provability-gaps.md"
FAIL=0

check_file() {
  local f="$1"
  [[ -f "$f" ]] || return 0
  if grep -qE 'proofs (do not|don.t) close|Lean must accept|binary iff Lean|proof certificate' "$f" &&
     ! grep -q "$GAPS" "$f"; then
    echo "FAIL $f: strong proof claim without link to $GAPS"
    FAIL=1
  fi
}

while IFS= read -r -d '' f; do
  check_file "$f"
done < <(find "$ROOT/docs" -name '*.md' -print0)

if [[ "$FAIL" -ne 0 ]]; then
  exit 1
fi
echo "check-doc-provability-claims: ok"
