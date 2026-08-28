# Benchmarks

Li ships a reproducible cross-language benchmark harness under `benchmarks/`.
Every tier-2 physics simulation uses **one shared C kernel** per benchmark;
`cpp`, `rust`, and `julia` labels run identical machine code, while `li` links
the same `.c` file through `lic` (`LI_EXTRA_C`).

## Tiers

| Tier | Scope | CI gate |
|------|-------|---------|
| 0 | `li-tests` + verify + MD stability (strict) | `./scripts/ci.sh` |
| 1 | Micro kernels (`simd_dot`, matmul, horner) | `./scripts/ci-bench.sh` |
| 2 | Physics sims (MD, N-body, wave, heat, pendulum) | manual / weekly workflow |

## Stability vs speed

- **Throughput** rows use low-density lattice params (`params.toml` `[perf]`).
- **Conservation** rows use `md_stress.c` at liquid density ρ=0.75.
- Strict gates: harmonic oscillator (Swope) + momentum invariant.
- Advisory gates: Allen–Tildesley energy MSD + timestep halving (not CI-blocking).

```bash
python3 benchmarks/harness/stability.py
python3 benchmarks/harness/bench.py --tier 12 --runs 3 --skip-verify
./scripts/plot_shareables.sh
```

See the [benchmarks implementation plan](superpowers/plans/2026-05-14-benchmarks-and-simulations.md)
for the full matrix and publication workflow.
