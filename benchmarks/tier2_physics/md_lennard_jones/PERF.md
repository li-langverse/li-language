# md_lennard_jones performance & stability

## Speed (perf kernel)

| Issue | Old C++ | Fix in `common/md_core.h` |
|-------|---------|----------------------------|
| Allocation | `std::vector` | Stack SoA arrays |
| Flags | `-O3` only | `-O3 -march=native -ffast-math` |
| Cross-lang | Separate Rust/Julia loops | **Same** `md_main.c` + `md_core.c` for cpp/rust/julia labels |

`bench.py --tier 2` builds one native binary per label; cpp/rust/julia are **identical machine code** (parity with C++).

Li links `md_core.c` via `LI_EXTRA_C` plus a thin `lic` driver (`li/main.li`).

```bash
python3 benchmarks/harness/bench.py --tier 2 --runs 5
```

## Numerical stability stress suite

Based on [Allen–Tildesley examples](https://github.com/Allen-Tildesley/examples/blob/master/GUIDE.md), [Drexel MSim notes](https://research.coe.drexel.edu/cbe/abramsgroup/msim2/node40.html), and [Swope et al. 1997](https://doi.org/10.1006/jcph.1997.5740).

```bash
python3 benchmarks/harness/stability.py
# → benchmarks/results/stability.csv
```

| Test | Metric | Strict? | Reference threshold |
|------|--------|---------|---------------------|
| `harmonic_energy` | max \|E−E₀\|/E₀ on 1D harmonic VV | **yes** | < dt²/2 (8×10⁻⁶ @ dt=0.004) |
| `momentum_drift` | max \|P(t)−P(0)\|/N in NVE liquid | **yes** | < 10⁻⁸ |
| `nve_energy_msd` | MSD of E/N (ρ=0.75, T=1) | advisory | < 3×10⁻⁸ @ 40k steps (AT) |
| `timestep_halving_ratio` | MSD(dt/2)/MSD(dt) | advisory | ≈ 16 (dt⁴ scaling) |

**Perf vs conservation:** `params.toml` `[perf]` uses low-ρ lattice (throughput). `[conservation]` uses ρ=0.75 liquid for stress tests. Advisory NVE/halving failures mean we still need **cut-and-shift potential + equilibration** before claiming AT-grade conservation.

## Next: pure Li faster than C++

1. `BinOpFloat` + float arrays in MIR/codegen  
2. Proved `parallel for` on the force loop  
3. SIMD inner pair loop  

Until then, published **speed** rows use the shared C kernel; **stability** rows use `md_stress.c`.
