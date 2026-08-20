#!/usr/bin/env bash
# Runtime gate: float-array element arithmetic must stay float end-to-end.
#
# Regression for the MIR `is_float_expr` gap: `a[0] * b[0]` on float arrays
# was lowered through BinOpInt (`fptosi` truncation) and the float return
# path read an uninitialized temp, so `dot4([1,1,1,1],[2,2,2,2])` returned
# garbage at runtime. The manifest harness only *builds*; this gate *runs*
# the binary and asserts the real values.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
TMP="$(mktemp -d)"
BIN="$TMP/float_array_codegen"
SRC="$TMP/float_array_codegen.li"

cat > "$SRC" <<'EOF'
# Runtime regression: float array element arithmetic + user-defined call.
# dot4 of [1,1,1,1] and [2,2,2,2] must be exactly 8.0 (no fptosi truncation).
def dot4(a: array[4, float], b: array[4, float]) -> float
  requires true
  ensures result >= 0.0
  decreases 0
=
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]

def main() -> int
  requires true
  ensures result == 0
  decreases 0
=
  var a: array[4, float]
  var b: array[4, float]
  var i: int = 0
  while i < 4
    a[i] = 1.0
    b[i] = 2.0
    i = i + 1
  # loop-indexed element arithmetic (the nanoreactor hot path)
  var acc: float = 0.0
  var j: int = 0
  while j < 4
    acc = acc + a[j] * b[j]
    j = j + 1
  if acc != 8.0:
    return 1
  # user-defined call returning float from float arrays
  var s: float = dot4(a, b)
  if s != 8.0:
    return 2
  return 0
EOF

"$LIC" build "$SRC" -o "$BIN" --allow-open-vc --no-lean-verify >/dev/null 2>&1
"$BIN"
rc=$?
rm -rf "$TMP"
if [[ $rc -ne 0 ]]; then
  echo "check_float_array_codegen_runtime: FAILED (rc=$rc)" >&2
  exit 1
fi
echo "check_float_array_codegen_runtime: ok (float array element arithmetic is exact at runtime)"
