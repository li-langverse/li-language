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
  "li-tests/math_linalg/matmul_chain_ok.li"
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
  # Full-corpus classification sweep (non-short-circuiting): bucket every
  # *.li by acceptance and, for both-accept files, byte-exactness. Evidence
  # for REAL-GAP files lands under ${MIR_OUT:-<tmp>} so divergences are
  # reproducible. Bucket semantics match the labels: CPP-ONLY means the C++
  # host accepts but the li walker rejects; LI-ONLY is the reverse (the
  # frontend-gap bucket the C++ still rejects).
  n_match=0; n_known=0; n_real=0; n_cpp_only=0; n_li_only=0; n_neither=0; n_total=0
  OUT="${MIR_OUT:-$TMP/sweep}"
  mkdir -p "$OUT"
  : > "$OUT/results.txt"
  while IFS= read -r f; do
    cpp=0; li=0
    "$LIC" mir "$f" >/dev/null 2>&1 && cpp=1
    "$LI"  mir "$f" >/dev/null 2>&1 && li=1
    rel="${f#$ROOT/}"
    n_total=$((n_total + 1))
    if [[ "$cpp" == "1" && "$li" == "0" ]]; then
      echo "CPP-ONLY $rel" >> "$OUT/results.txt"; n_cpp_only=$((n_cpp_only + 1)); continue
    fi
    if [[ "$cpp" == "0" && "$li" == "1" ]]; then
      echo "LI-ONLY $rel" >> "$OUT/results.txt"; n_li_only=$((n_li_only + 1)); continue
    fi
    if [[ "$cpp" == "0" && "$li" == "0" ]]; then
      echo "NEITHER $rel" >> "$OUT/results.txt"; n_neither=$((n_neither + 1)); continue
    fi
    "$LIC" mir "$f" > "$OUT/cpp.mir" 2>/dev/null
    "$LI"  mir "$f" > "$OUT/li.mir"  2>/dev/null
    if diff -q "$OUT/cpp.mir" "$OUT/li.mir" >/dev/null 2>&1; then
      echo "MATCH $rel" >> "$OUT/results.txt"; n_match=$((n_match + 1)); continue
    fi
    # Known intentional diff: C++ suppresses the post-terminator merge jump
    # (INS 44) after if/else where the walker still emits it. Signature: every
    # diff hunk is a '<' Jump line present only in the walker (diff order is
    # li.mir then cpp.mir), with no '>' side carrying a Jump.
    difftxt="$(diff "$OUT/li.mir" "$OUT/cpp.mir" || true)"
    add_only="$(printf '%s\n' "$difftxt" | grep '^<' | grep -c 'INS 44' || true)"
    del_only="$(printf '%s\n' "$difftxt" | grep '^>' | grep -c 'INS 44' || true)"
    total_lines="$(printf '%s\n' "$difftxt" | grep -c '^[<>]' || true)"
    if [[ "$add_only" -gt 0 && "$del_only" == "0" && "$total_lines" == "$add_only" ]]; then
      echo "KNOWN-DIFF $rel" >> "$OUT/results.txt"; n_known=$((n_known + 1))
    else
      echo "REAL-GAP $rel" >> "$OUT/results.txt"; n_real=$((n_real + 1))
      cp "$OUT/li.mir" "$OUT/$(echo "$rel" | tr '/' '_').li.mir"
      cp "$OUT/cpp.mir" "$OUT/$(echo "$rel" | tr '/' '_').cpp.mir"
    fi
  done < <(find "$ROOT/bootstrap" "$ROOT/examples" "$ROOT/li-tests" "$ROOT/packages" \
             "$ROOT/proof-db" -name "*.li" -type f \
             -not -path "$ROOT/bootstrap/lic/main.li" \
             2>/dev/null | sort)
  echo "match=$n_match known-diff=$n_known real-gap=$n_real cpp-only=$n_cpp_only li-only=$n_li_only neither=$n_neither (total $n_total)" \
    | tee "$OUT/summary.txt"
fi

echo "check_li_mir_parity: ok (${#CORPUS[@]} corpus files, byte-exact MIR dumps)"
