# Floating-point numerical stability (optional)

## Summary

**LLVM 22 does not provide a general “make this expression numerically stable” optimization pass.** It optimizes for speed when you enable fast-math (`-ffast-math`, `reassoc`, `contract`, etc.) and otherwise mostly preserves IEEE-754 evaluation order for scalar FP.

Li therefore implements an **optional compiler mode** that runs **before** LLVM:

```bash
lic build app.li -o app --numerically-stable
# or
LI_FP_NUMERICALLY_STABLE=1 lic build app.li -o app
```

This is complementary to [physics numerical policy](../physics/numerical-policy.md) (integrator *selection*), not a replacement.

## What the flag does

| Layer | Behavior |
|-------|----------|
| **MIR pass** | Algebraic rewrites for cancellation-prone forms |
| **Codegen** | Kahan summation for `sum(float[])`; strict FP math flags on scalar ops |
| **Link** | `-fno-fast-math -ffp-contract=off` so Clang/LLVM do not reassociate away stability |

### MIR rewrites (v1)

| Pattern | Rewrite |
|---------|---------|
| `x*x - y*y` | `(x - y) * (x + y)` |
| `(a + b) - c` | `a - (c - b)` |
| `sqrt(x*x + y*y)` | `li_rt_hypot(x, y)` |
| `li_rt_exp(x) - 1` | `li_rt_expm1(x)` |
| `li_rt_log(1 + x)` | `li_rt_log1p(x)` |

### Compensated reductions

With `--numerically-stable`, array `sum` on `float`/`f64` arrays uses **Kahan summation** in the LLVM loop.

## What LLVM still does not do for us

- No automatic detection of arbitrary `a - b` cancellation when `a ≈ b`
- No Herbie-style expression search across equivalent formulas
- No interval/arbitrary-precision proof of error bounds

For those, keep using `math.numerics` / `li-math-numerics`, explicit `requires`/`ensures`, and Tier-0 stability benches.

## Benchmarks note

C++ reference kernels in `benchmarks/` may use `-ffast-math`. Li **does not** enable fast-math by default. Use `--numerically-stable` for physics-grade Li builds; compare fairly against a C++ build **without** `-ffast-math` when measuring drift, not peak FLOPS.

## Related runtime symbols

- `li_rt_hypot`, `li_rt_expm1`, `li_rt_log1p` — `runtime/li_rt.c`
