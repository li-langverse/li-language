---
name: research-li-numerics
description: >-
  Research numerical methods and approximations for Li physics packages.
  Survey references, pick integrators/solvers for a fidelity tier, document
  error bounds, and wire Tier-0 stability checks before merging kernels.
---

# Research Li numerics

Use when adding or changing physics integrators, PDE stencils, solvers, or compile-time numerical policy.

## Before proposing a method

1. Read [docs/physics/numerical-policy.md](../../../docs/physics/numerical-policy.md)
2. Read [docs/physics/overview.md](../../../docs/physics/overview.md)
3. Check existing code in `packages/li-std-numerics` and `benchmarks/tier2_physics/`
4. Run / extend `benchmarks/harness/stability.py` invariants for the problem class

## Research checklist

- [ ] **Learned from** 2–4 references (textbook, Gaffer on Games, CFD notes, MD literature)
- [ ] State **problem class** (ODE, elliptic, hyperbolic, stochastic, quantum)
- [ ] Pick **tier** (T0–T3) and justify cost vs accuracy
- [ ] Document **stable timestep** rule (CFL, symplectic dt, etc.)
- [ ] Provide **reference solution** (analytic, golden checksum, or smaller-dt convergence)
- [ ] Add **`params.toml`** shared across cpp/rust/li if adding a bench
- [ ] Map to **`NumericalTargets`** fields in `li-std-physics-core`

## Compile-time selection (planned)

When language support lands (**PHY-n**), methods must be chosen from tables in `numerical-policy.md` — not ad-hoc `extern` C in user-facing APIs.

## Deliverables per kernel

| Artifact | Location |
|----------|----------|
| Library API | `packages/li-std-physics-*/src/lib.li` |
| Pure-Li driver | `benchmarks/tier2_physics/<id>/li/main.li` |
| Shared reference C (optional) | `benchmarks/tier2_physics/<id>/common/*_core.c` |
| Catalog row | `li-langverse/benchmarks/catalog.toml` |
| Stability row | `benchmarks/harness/stability.py` |

## Do not

- Ship user-facing `__li_simd_*` in physics stdlib
- Claim Lean proves energy bounds without updating [provability-gaps.md](../../../docs/verification/provability-gaps.md)
- Skip Tier-0 correctness for speed
