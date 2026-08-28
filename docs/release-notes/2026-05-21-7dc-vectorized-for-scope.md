# Phase 7d-c — `@vectorized` on `for` enables scoped array SIMD

## Summary

`@vectorized(lanes=4)` on a `for` loop emits `ArraySimdScope` MIR so array `@` / element-wise ops inside the body use f64×4 codegen even when the enclosing `def` has `@no_vectorize`.

## Agent continuation

1. **Read** `ArraySimdScope` in `mir.hpp`, `lower.cpp` `For` lowering, `emit.cpp` scope stack.
2. **Run** `./li-tests/run_all.sh` (**172** pass).
3. **Then** benchmarks ingest; `**` on arrays (**2i**).
4. **Blocked on** auto-detecting reduction loops without explicit `@vectorized`.

## Changed

- `compiler/mir/`, `compiler/codegen/emit.cpp`
- `li-tests/decorators/vectorized_for_scope_ok.li`

## Not changed

- `@parallel` on `for`; matmul auto-SIMD inner loops.

## Breaking

N/A.

## Security

N/A.

## Performance

Inner-loop array ops regain SIMD under `@no_vectorize` + `@vectorized for`.

## Downstream

- AXPY-style loops can annotate `@vectorized` on the serial `for`.
