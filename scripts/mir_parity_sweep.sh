#!/usr/bin/env bash
# MIR parity sweep: classify every corpus .li as match / known-diff / real-gap.
# Non-short-circuiting variant of scripts/check_li_mir_parity.sh (same binaries,
# same find-set), preserving per-file evidence under /tmp/mir-parity.
set -u
ROOT="/Users/julian/Documents/coding-projects/lic-gap2-proofdb"
CPP="$ROOT/build/compiler/lic/lic"
LI=/tmp/li-walker-mir
OUT=/tmp/mir-parity
mkdir -p "$OUT"
: > "$OUT/results.txt"

n_match=0; n_known=0; n_real=0; n_cpp_only=0; n_li_only=0; n_neither=0

while IFS= read -r f; do
  rel="${f#$ROOT/}"
  cpp=0; li=0
  "$CPP" mir "$f" >/dev/null 2>&1 && cpp=1
  "$LI"  mir "$f" >/dev/null 2>&1 && li=1
  if [[ "$cpp" == 1 && "$li" == 0 ]]; then
    echo "LI-ONLY $rel" >> "$OUT/results.txt"; n_li_only=$((n_li_only+1)); continue
  fi
  if [[ "$cpp" == 0 && "$li" == 1 ]]; then
    echo "CPP-ONLY $rel" >> "$OUT/results.txt"; n_cpp_only=$((n_cpp_only+1)); continue
  fi
  if [[ "$cpp" == 0 && "$li" == 0 ]]; then
    echo "NEITHER $rel" >> "$OUT/results.txt"; n_neither=$((n_neither+1)); continue
  fi
  "$CPP" mir "$f" > "$OUT/cpp.mir" 2>/dev/null
  "$LI"  mir "$f" > "$OUT/li.mir"  2>/dev/null
  if diff -q "$OUT/cpp.mir" "$OUT/li.mir" >/dev/null 2>&1; then
    echo "MATCH $rel" >> "$OUT/results.txt"; n_match=$((n_match+1)); continue
  fi
  # Diverges: classify by diff shape.
  # Known intentional diff: C++ suppresses the post-terminator merge jump
  # (INS 44) after if/else where the walker still emits it. Signature: every
  # diff hunk is a '<' C++ Jump line that the walker lacks (no '>' side with
  # a Jump).
  difftxt="$(diff "$OUT/li.mir" "$OUT/cpp.mir")"
  add_only="$(printf '%s\n' "$difftxt" | grep '^<' | grep -c 'INS 44' || true)"
  del_only="$(printf '%s\n' "$difftxt" | grep '^>' | grep -c 'INS 44' || true)"
  total_lines="$(printf '%s\n' "$difftxt" | grep -c '^[<>]' || true)"
  if [[ "$add_only" -gt 0 && "$del_only" == 0 && "$total_lines" == "$add_only" ]]; then
    echo "KNOWN-DIFF $rel" >> "$OUT/results.txt"; n_known=$((n_known+1)); continue
  else
    echo "REAL-GAP $rel" >> "$OUT/results.txt"
    cp "$OUT/li.mir" "$OUT/$(echo "$rel" | tr '/' '_').li.mir"
    cp "$OUT/cpp.mir" "$OUT/$(echo "$rel" | tr '/' '_').cpp.mir"
    n_real=$((n_real+1))
  fi
done < <(find "$ROOT/bootstrap" "$ROOT/examples" "$ROOT/li-tests" "$ROOT/packages" \
           "$ROOT/proof-db" -name "*.li" -type f \
           -not -path "$ROOT/bootstrap/lic/main.li" 2>/dev/null | sort)

echo "match=$n_match known-diff=$n_known real-gap=$n_real cpp-only=$n_cpp_only li-only=$n_li_only neither=$n_neither" \
  | tee "$OUT/summary.txt"
echo "---"
grep -c . "$OUT/results.txt"
