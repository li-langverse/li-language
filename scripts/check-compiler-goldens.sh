#!/usr/bin/env bash
# Compiler-only golden gate. Package runtimes and product packages are tested
# separately; this gate covers language lowering and core runtime ABI only.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIC="${LIC:-$ROOT/build/compiler/lic/lic}"
[[ -x "$LIC" ]] || { echo "check-compiler-goldens: missing lic: $LIC" >&2; exit 1; }

corpus=(
  li-tests/typecheck/fib.li
  li-tests/runtime/unary_minus.li
  li-tests/codegen/neg_float_procs.li
  examples/hello.li
  examples/arrays.li
  li-tests/effects/io_ok.li
  li-tests/contracts_verify/linalg_dot4_float_closed.li
  li-tests/contracts_verify/linalg_mat2_at2_float_closed.li
  li-tests/contracts_verify/linalg_mat2_float_value.li
  li-tests/contracts_verify/linalg_mat2_callproc_float_closed.li
  li-tests/math_linalg/vec_add_sum.li
  li-tests/math_linalg/elementwise_mul_float.li
  li-tests/math_linalg/broadcast_len1_add_float4.li
  li-tests/math_linalg/sum_elementwise_product.li
  li-tests/math_linalg/dot_float_arrays.li
  li-tests/math_linalg/elementwise_mul_float10.li
  li-tests/math_linalg/broadcast_len1_pow_int4.li
  li-tests/math_linalg/broadcast_len1_mul_int4.li
  li-tests/math_linalg/axpy_float4.li
  li-tests/math_linalg/scale_float4.li
  li-tests/math_linalg/norm_float4.li
  li-tests/math_linalg/norm_int4_sq.li
  li-tests/decorators/vectorized_dot_ok.li
  li-tests/decorators/cpu_only_ok.li
  li-tests/decorators/no_vectorize_dot_ok.li
  li-tests/decorators/vectorized_dot_proc_ok.li
  li-tests/effects/async_ok.li
  li-tests/async/await_codegen_ok.li
  li-tests/math_linalg/matmul_2x3_ok.li
  li-tests/math_linalg/matmul_chain_ok.li
  li-tests/decorators/vectorized_for_scope_ok.li
  li-tests/decorators/vectorized_for_parse_ok.li
  li-tests/physics/golden_positions_sum.li
  li-tests/objects/object_field_smoke.li
  li-tests/objects/object_copy_init.li
  li-tests/contracts_verify/http_parse_forward_closed.li
)

for rel in "${corpus[@]}"; do
  "$LIC" mir "$ROOT/$rel" >/dev/null || {
    echo "check-compiler-goldens: FAIL $rel" >&2
    exit 1
  }
done

echo "check-compiler-goldens: ok (${#corpus[@]} compiler-only lowering goldens)"
