# Phase 2i-a — Element-wise array arithmetic

## Summary

Matching 1d `array[N, int]` / `array[N, float]` operands support `+`, `-`, `*`, `/` with a new array result; `sum(a * b)` lowers via a temp product array.

## Agent continuation

1. **Read** `li-tests/math_linalg/elementwise_mul_float.li`, `sum_elementwise_product.li`.
2. **Run** `cmake --build build && ./li-tests/run_all.sh` (expect **165** pass).
3. **Then** merge PR stack through **#142**; optional SIMD for `ArrayBinOpF64` lanes.
4. **Blocked on** broadcast / mismatched-length promotion (not in v1).

## Changed

| Path | What |
|------|------|
| `compiler/types/typecheck.cpp` | Element-wise array binop types |
| `compiler/mir/` | `ArrayBinOpF64`, `ArrayBinOpI64`; `sum(expr)` |
| `compiler/codegen/emit.cpp` | Per-element loops |
| `li-tests/math_linalg/elementwise_*.li`, `sum_elementwise_product.li` |

## Not changed

- `**` on arrays; `%` / `//` on arrays.
- 2d matrix element-wise.
- `@vectorized` auto-SIMD.

## Breaking

N/A.

## Security

N/A.

## Performance

Scalar element loops; LLVM may autovec at `-O3`.

## Downstream

- `docs/guide/math-hpc-examples.md` — `sum(a * b)` row updated.
- Tier 1 `simd_dot` could use `sum(a * b)` style in a follow-up bench refresh.
