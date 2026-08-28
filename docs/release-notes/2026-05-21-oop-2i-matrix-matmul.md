# Phase 2i-c — 2D float matrix `@`

## Summary

`A @ B` for nested `array[M, array[K, float]]` operands is shape-checked at compile time and lowered to a triple-loop `ArrayMatMul2DF64` MIR op with LLVM `[M × [N × f64]]` stack storage.

## Agent continuation

1. **Read** master plan **7e-b** (Tier 1 matmul benches with math-only Li source) and `docs/superpowers/plans/2026-05-16-li-math-linalg-surface.md`.
2. **Run** `LI_REPO_ROOT=$PWD cmake --build build && LI_REPO_ROOT=$PWD ./li-tests/run_all.sh` (expect **161** pass).
3. **Then** wire `benchmarks` tier-1 `matmul_*` to pure `@` Li kernels (no `__li_simd_*` in user files); or stack-merge **2j** PR #137 before this branch.
4. **Blocked on** SIMD auto-lowering (**7e-a**) and `tensor[(M,N), f64]` types (Phase 3).

## Changed

| Path | What |
|------|------|
| `compiler/types/typecheck.cpp` | `ty_is_2d_float_matrix`, `A[M,K] @ B[K,N]` result type |
| `compiler/mir/include/li/mir.hpp` | `ArrayLoad2DF64`, `ArrayStore2DF64`, `ArrayMatMul2DF64` |
| `compiler/mir/lower.cpp` | Matrix alloc, nested `A[i][j]`, `C = A @ B` |
| `compiler/codegen/emit.cpp` | 2D LLVM arrays + matmul / load / store |
| `li-tests/math_linalg/matmul_2x3_ok.li` | pass |
| `li-tests/math_linalg/matmul_dim_mismatch.li` | compile_fail (inner dim) |

## Not changed

- SIMD / `@vectorized` auto-lowering on matmul loops (**7e-a**).
- `C += A @ B` in-place accumulate syntax.
- Dynamic / runtime-sized matrices; only fixed literal shapes.
- Httpd, OOP method VCs beyond existing **2j-f** stack.

## Breaking

N/A — new capability; 1d `@` dot unchanged.

## Security

N/A — compile-time fixed shapes; no new trusted surface.

## Performance

Naive O(M×N×K) scalar loops; no blocking or SIMD yet. Bench threshold N/A until **7e-b**.

## Downstream

- `docs/language/linear-algebra.md` — documents 2d `@`.
- Master plan **2i** checkbox: matrix `@` v1 landed.
