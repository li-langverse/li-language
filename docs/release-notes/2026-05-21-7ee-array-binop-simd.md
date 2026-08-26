# Phase 7e-e — Safe f64×4 SIMD for `ArrayBinOpF64`

## Summary

Element-wise `+ - * /` on `array[N, float]` uses the same insertelement gather / extractelement scatter pattern as `ArrayDotF64`, with a scalar tail when `N % 4 ≠ 0`.

## Agent continuation

1. **Read** `compiler/codegen/emit.cpp` — `scatter_array_f64x4`, `ArrayBinOpF64`.
2. **Run** `./li-tests/run_all.sh` (expect **166** pass).
3. **Then** `@vectorized` decorator lowering (7d) or `sum(a*b)` in `simd_dot` bench refresh.
4. **Blocked on** `ArrayBinOpI64` SIMD (low priority).

## Changed

| Path | What |
|------|------|
| `compiler/codegen/emit.cpp` | `scatter_array_f64x4`; SIMD main loop in `ArrayBinOpF64` |
| `li-tests/math_linalg/elementwise_mul_float10.li` | N=10 (8 SIMD + 2 tail) |

## Not changed

- `ArrayBinOpI64` (scalar).
- `ArrayMatMul2DF64` SIMD.
- Decorator-driven vectorization.

## Breaking

N/A.

## Security

N/A.

## Performance

4-wide vector ops for `N ≥ 4`; benefits `sum(a * b)` product temp.

## Downstream

- Tier 1 `simd_dot` could switch to `sum(a * b)` for clearer math surface.
