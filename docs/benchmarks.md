# Benchmarks

Li ships a reproducible cross-language benchmark harness under `benchmarks/`.
Every tier-2 physics simulation uses **one shared C kernel** per benchmark;
`cpp`, `rust`, and `julia` labels run identical machine code, while `li` links
the same `.c` file through `lic` (`LI_EXTRA_C`).

## Modular scope (package-scoped runs)

Changing one workspace package should **not** rerun the full tier-1/2 matrix.

| Tool | Purpose |
|------|---------|
| `benchmarks/manifest.toml` | Maps `packages/*` → benchmark ids, composables, hooks |
| `benchmarks/harness/bench_scope.py` | Resolve scope: `--package`, `--changed`, `--print-benches` |
| `./scripts/bench-package.sh` | Run scoped verify, composables, optional `--timing` |
| `bench.py --only` / `--package` / `--changed` | Merge timings for subset into `latest.csv` |
| `bench_sim.py` | Sim registry + summaries + selective tier-2 smokes |

```bash
./scripts/bench-package.sh li-sim-scientific --write-summary
./scripts/bench-package.sh --changed --timing --runs 1
python3 benchmarks/harness/bench.py --tier 2 --only md_lennard_jones,heat_equation_2d
```

Sim agents: [sim-agent-handoff.md](ecosystem/sim-agent-handoff.md).

## Tiers

| Tier | Scope | CI gate |
|------|-------|---------|
| 0 | `li-tests` + `verify.py` tier-0 `lic build` + MD stability (strict) | `./scripts/ci.sh` |
| 1 | Micro kernels (`simd_dot`, matmul, horner) | `./scripts/ci-bench.sh` |
| 2 | Physics sims (MD, N-body, wave, heat, pendulum) | manual / weekly workflow |
| 3 | HTTP / li-httpd vs nginx, apache, node, … | `benchmarks/scripts/run-full-benchmark-suite.sh` |
| 4 | HTTP exploit grid (CVE-style harness) | same suite (`SKIP_EXPLOITS=1` to skip) |
| 5 | Ecosystem: compile time, lip/lit smoke, security timing | **deferred** — `RUN_TIER5_ECOSYSTEM=1` |

## Stability vs speed

- **Throughput** rows use low-density lattice params (`params.toml` `[perf]`).
- **Conservation** rows use `md_stress.c` at liquid density ρ=0.75.
- Strict gates: harmonic oscillator (Swope) + momentum invariant.
- Advisory gates: Allen–Tildesley energy MSD + timestep halving (not CI-blocking).

```bash
./scripts/check-bench-harness-contract.sh   # verify/timing contract + tier-0 smoke (needs lic)
python3 benchmarks/harness/verify.py        # tier-0 only (same smokes as bench --tier 0)
python3 benchmarks/harness/stability.py
python3 benchmarks/harness/bench.py --tier 12 --runs 3 --skip-verify
# Optional when ecosystem benches are ready:
# RUN_TIER5_ECOSYSTEM=1 python3 benchmarks/harness/bench_ecosystem.py --runs 3
./scripts/plot_shareables.sh
```

## Dashboard ([li-langverse/benchmarks](https://github.com/li-langverse/benchmarks))

CI on `lic` `dev`/`main` uploads `benchmarks/results/latest.csv` (+ `stability.csv`, `security.csv`) and dispatches ingest.
Add new benchmark ids to the org **`catalog.toml`** — see [benchmarks-catalog-additions.toml](ecosystem/benchmarks-catalog-additions.toml) for tier-3 rows (async, effects, security).

Published site: [li-langverse.github.io/benchmarks](https://li-langverse.github.io/benchmarks/).

See the [benchmarks implementation plan](superpowers/plans/2026-05-14-benchmarks-and-simulations.md)
for the full matrix and publication workflow.
