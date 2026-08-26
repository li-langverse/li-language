# Phase 7e-c (partial) — Math-first HPC documentation

## Summary

Adds `docs/guide/math-hpc-examples.md` and refreshes handbook pages so users and agents write `dot` / `@` / `sum` — not `__li_simd_*` — with honest status tables for what compiles today.

## Agent continuation

1. **Read** `docs/guide/math-hpc-examples.md` before editing Tier 1 bench Li sources.
2. **Run** `./li-tests/run_all.sh` (no compiler change; docs-only).
3. **Then** merge PR stack **#138 → #139 → #141 → this**; or implement element-wise array `*` (**2i-a**) + `sum(a*b)`.
4. **Blocked on** `@vectorized` elaboration (**7d**) for AXPY auto-SIMD.

## Changed

| Path | What |
|------|------|
| `docs/guide/math-hpc-examples.md` | New — dot, AXPY loop, 2d `@`, sum, MD sketch, audit table |
| `docs/language/linear-algebra.md` | Implemented vs planned table |
| `docs/guide/fast-math-and-parallelism.md` | Math-first; intrinsics moved to appendix |
| `docs/guide/examples-gallery.md` | Math vs intrinsics side-by-side |
| `README.md` | Hello math `x @ y` sample |
| `mkdocs.yml` | Nav entry for math-hpc-examples |

## Not changed

- Compiler lowering (7e-a/b code unchanged).
- `docs/guide/math-hpc-examples.md` does not claim `sum(a * b)` works yet.
- `matmul_blocked` pure-Li docs (still C kernel in bench).

## Breaking

N/A — documentation only.

## Security

N/A.

## Performance

N/A.

## Downstream

- Agents triaging HPC issues should cite math guide before suggesting intrinsics.
- Benchmarks dashboard copy can link `math-hpc-examples` for `pure_li` column legend.
