# Phase 7e-a (partial) — Math dot surface + `simd_dot` pure Li

## Summary

Tier 1 `simd_dot` Li source uses `a @ b` on fixed `array[40000, float]` tiles (no `__li_simd_*`). Prelude `dot(a, b)` aliases the same lowering; `ArrayDotF64` codegen remains scalar (LLVM may autovec at `-O3`).

## Agent continuation

1. **Read** `docs/release-notes/2026-05-21-7eb-matmul-pure-li.md` and merge stack **#138 → #139 → this PR**.
2. **Run** `./li-tests/run_all.sh` (162 pass) and `python3 benchmarks/harness/bench.py` verify for `simd_dot`.
3. **Then** fix explicit f64×4 `ArrayDotF64` SIMD loads (insert-element path; vector `load` segfaulted on LLVM 22), or **7e-c** docs gallery.
4. **Blocked on** `@vectorized` decorator lowering to MIR (7d elaboration).

## Changed

| Path | What |
|------|------|
| `benchmarks/tier1_micro/simd_dot/li/main.li` | 800× `dot_tile_f64(rep)` with `return a @ b` |
| `benchmarks/harness/bench.py` | `simd_dot` `flops_per_run=8e7` (800 tiles) |
| `compiler/types/typecheck.cpp` | `dot()` builtin; skip `check_call_args` for prelude |
| `compiler/mir/lower.cpp` | `lower_float_array_dot_f64`, `dot()` call |
| `compiler/types/prelude.cpp` | `dot` in prelude proc set |
| `li-tests/math_linalg/dot_float_arrays.li` | `@` vs `dot()` |

## Not changed

- Explicit SIMD MIR for `ArrayDotF64` (vector load caused runtime SIGSEGV; reverted).
- `sum(a * b)` element-wise (no array `*` yet).
- `matmul_blocked` pure Li.
- `@vectorized` on loops.

## Breaking

N/A.

## Security

N/A.

## Performance

`simd_dot` pure-Li ~0.9× native wall on smoke run (800×40k `@`); not parity-gated. Native column still uses C `dot_core.c`.

## Downstream

- Master plan **7e-a** checkbox: bench source clean; SIMD MIR explicit lanes follow-up.
