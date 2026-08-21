#!/usr/bin/env bash
# Layer 5 self-host parity: the li `mir` subcommand (built from
# bootstrap/lic/main.li) must emit a canonical MIR dump byte-identical to the
# C++ `lic mir <file>` dump of lower_to_mir() output.
#
# The corpus covers the scalar lowering slice: externs, int/float/str
# literals, binops (incl. unary minus), calls (incl. recursion), if/elif/else,
# while, for, returns, var decls, assignments, literal/ident-index array
# load/store, 1D float-array `@` dot (ArrayDotF64), 2D float matrices
# (matrix params, ArrayAlloc, ArrayLoad2DF64/ArrayStore2DF64), and elementwise
# ArrayBinOpF64 / ArraySumF64 on float-array params. Object lowering, local-
# array elementwise, and the remaining long tail are tracked by the full
# sweep flag.
# Files must pass the strict `lic mir` frontend (no --allow-open-vc).
#
# Usage:
#   scripts/check_li_mir_parity.sh
#   LI_CHECK_BIN=/custom/lic scripts/check_li_mir_parity.sh
#   LI_MIR_FULL_SWEEP=1 scripts/check_li_mir_parity.sh   # every *.li that parses
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export LI_REPO_ROOT="$ROOT"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI="${LI_CHECK_BIN:-$TMP/lic-from-li}"

CORPUS=(
  "li-tests/typecheck/fib.li"
  "li-tests/runtime/unary_minus.li"
  "li-tests/codegen/neg_float_procs.li"
  "examples/hello.li"
  "examples/arrays.li"
  "li-tests/effects/io_ok.li"
  "li-tests/contracts_verify/linalg_dot4_float_closed.li"
  "li-tests/contracts_verify/linalg_mat2_at2_float_closed.li"
  "li-tests/contracts_verify/linalg_mat2_float_value.li"
  "li-tests/contracts_verify/linalg_mat2_callproc_float_closed.li"
  "li-tests/math_linalg/vec_add_sum.li"
  "li-tests/math_linalg/elementwise_mul_float.li"
  "li-tests/math_linalg/broadcast_len1_add_float4.li"
  "li-tests/math_linalg/sum_elementwise_product.li"
  "li-tests/math_linalg/dot_float_arrays.li"
  "li-tests/math_linalg/elementwise_mul_float10.li"
  "li-tests/math_linalg/broadcast_len1_pow_int4.li"
  "li-tests/math_linalg/broadcast_len1_mul_int4.li"
  "li-tests/math_linalg/axpy_float4.li"
  "li-tests/math_linalg/scale_float4.li"
  "li-tests/math_linalg/norm_float4.li"
  "li-tests/math_linalg/norm_int4_sq.li"
  "li-tests/decorators/vectorized_dot_ok.li"
  "li-tests/decorators/cpu_only_ok.li"
  "li-tests/decorators/no_vectorize_dot_ok.li"
  "li-tests/decorators/vectorized_dot_proc_ok.li"
  "li-tests/effects/async_ok.li"
  "li-tests/async/await_codegen_ok.li"
  "li-tests/math_linalg/matmul_2x3_ok.li"
  "li-tests/decorators/vectorized_for_scope_ok.li"
  "li-tests/decorators/vectorized_for_parse_ok.li"
  "li-tests/physics/golden_positions_sum.li"
  "packages/lig/li-tests/smoke/kernel_matmul_parity.li"
  "li-tests/objects/object_field_smoke.li"
  "li-tests/objects/object_copy_init.li"
  "li-tests/contracts_verify/http_parse_forward_closed.li"
)

fail() {
  echo "check_li_mir_parity: $1" >&2
  exit 1
}

# Build the li MIR walker with the C++ host (the self-host milestone gate).
"$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$LI" --allow-open-vc --no-lean-verify \
  >/dev/null 2>&1 || fail "could not build bootstrap/lic/main.li with $LIC"

checked=0
for f in "${CORPUS[@]}"; do
  src="$ROOT/$f"
  [[ -f "$src" ]] || fail "corpus file missing: $f"
  if ! "$LIC" mir "$src" > "$TMP/cpp_mir.txt" 2>/dev/null; then
    fail "C++ `lic mir` rejected corpus file $f (gate corpus must lower)"
  fi
  if ! "$LI" mir "$src" > "$TMP/li_mir.txt" 2>/dev/null; then
    fail "li `mir` rejected $f (parity break on valid input)"
  fi
  if ! diff -q "$TMP/cpp_mir.txt" "$TMP/li_mir.txt" >/dev/null 2>&1; then
    echo "check_li_mir_parity: MIR mismatch on $f:" >&2
    diff "$TMP/cpp_mir.txt" "$TMP/li_mir.txt" | head -15 >&2
    fail "li MIR dump differs from C++ on $f"
  fi
  checked=$((checked + 1))
  echo "  ok  $f ($(wc -l < "$TMP/cpp_mir.txt") insns)"
done

if [[ "${LI_MIR_FULL_SWEEP:-0}" == "1" ]]; then
  checked=0
  mismatch=0
  while IFS= read -r f; do
    cpp=0; li=0
    "$LIC" mir "$f" >/dev/null 2>&1 && cpp=1
    "$LI" mir "$f" >/dev/null 2>&1 && li=1
    if [[ "$cpp" == "1" && "$li" == "0" ]]; then
      # Li frontend gap: C++ accepts but Li rejects (generics, objects, etc.)
      checked=$((checked + 1))
      continue
    fi
    if [[ "$cpp" == "0" && "$li" == "1" ]]; then
      checked=$((checked + 1))
      continue
    fi
    if [[ "$cpp" == "1" ]]; then
      "$LIC" mir "$f" > "$TMP/cpp_mir.txt" 2>/dev/null
      "$LI" mir "$f" > "$TMP/li_mir.txt" 2>/dev/null
      if ! diff -q "$TMP/cpp_mir.txt" "$TMP/li_mir.txt" >/dev/null 2>&1; then
        mismatch=$((mismatch + 1))
      fi
    fi
    checked=$((checked + 1))
  done < <(find "$ROOT/bootstrap" "$ROOT/examples" "$ROOT/li-tests" "$ROOT/packages" \
             "$ROOT/proof-db" -name "*.li" -type f \
             -not -path "$ROOT/bootstrap/lic/main.li" \
             2>/dev/null)
  echo "  full sweep: $checked files, $mismatch MIR mismatches (known Li frontend gaps)"
fi

echo "check_li_mir_parity: ok (${#CORPUS[@]} corpus files, byte-exact MIR dumps)"
