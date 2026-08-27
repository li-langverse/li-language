# Phase 7e-d — Safe f64×4 gather for `ArrayDotF64`

## Summary

`ArrayDotF64` codegen uses scalar loads + `insertelement` into `f64x4`, vector multiply-add in 4-wide chunks, horizontal sum, and a scalar tail — no wide vector load from stack allocas.

## Agent continuation

1. **Read** `compiler/codegen/emit.cpp` — `gather_array_f64x4`, `ArrayDotF64`.
2. **Run** `cmake --build build && ./li-tests/run_all.sh` (expect **165** pass); optional `benchmarks/harness/bench.py` on `simd_dot`.
3. **Then** same pattern for `ArrayBinOpF64` if bench gains warrant; `@vectorized` decorator lowering (7d).
4. **Blocked on** broadcast element-wise SIMD; GPU paths.

## Changed

| Path | What |
|------|------|
| `compiler/codegen/emit.cpp` | `gather_array_f64x4`, `horiz_sum_f64x4`; SIMD main loop + scalar tail in `ArrayDotF64` |

## Not changed

- `ArrayBinOpF64` (still scalar per element).
- MIR / typecheck.
- Tier 1 bench sources.

## Breaking

N/A.

## Security

N/A.

## Performance

4-wide SIMD chunk for `n ≥ 4`; tail scalar. Avoids prior `CreateLoad(vec4)` on `[N x f64]` alloca SIGSEGV.

## Downstream

- `simd_dot` Tier 1 may improve vs scalar-only; re-run `bench.py` after merge stack.
