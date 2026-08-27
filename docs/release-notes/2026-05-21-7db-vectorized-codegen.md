# Phase 7d-b — `@vectorized` / `@no_vectorize` array SIMD gates

## Summary

`@no_vectorize` on a `def` forces scalar loops for `ArrayDotF64` / `ArrayBinOpF64`; `@vectorized(lanes=4)` is validated and documents intent; `lanes=8` is a compile error (E0322). `@vectorized` may prefix `for` loops (parse only).

## Agent continuation

1. **Read** `MirFn.no_vectorize`, `EmitCtx.enable_array_simd`, `check_proc_decorators`.
2. **Run** `./li-tests/run_all.sh` (expect **170** pass).
3. **Then** elaborate loop-body `@vectorized` to MIR (not just parse); `@parallel` + math loops.
4. **Blocked on** f64x8 codegen for `lanes=8`.

## Changed

| Path | What |
|------|------|
| `compiler/mir/` | `MirDecorator.lanes`, `MirFn.no_vectorize` |
| `compiler/codegen/emit.cpp` | `enable_array_simd` gate |
| `compiler/types/policy_module.cpp` | E0322 lanes policy |
| `compiler/parser/parser.cpp` | `@… for` parse |
| `li-tests/decorators/vectorized_*.li` | four tests |

## Not changed

- Loop-body lowering from `@vectorized` (metadata on `for` only).
- `@parallel` on math `@` / matmul.
- f64x8 vectors.

## Breaking

N/A.

## Security

N/A.

## Performance

Default remains f64x4 SIMD for array ops; opt out with `@no_vectorize`.

## Downstream

- AXPY examples can annotate `@vectorized(lanes=4)` on `def`.
