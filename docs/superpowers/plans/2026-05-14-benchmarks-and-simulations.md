# Li Benchmarks & Simulation Test Plan

> **For agentic workers:** Implement harness first (Phase 5b), then add simulations as language features land. Correctness before speed; cross-language only after li reference impl is trusted.

**Goal:** A reproducible `benchmarks/` suite: physics simulations (MD, N-body, PDEs), ML micro/kernels, and published comparisons vs C++, Rust, Python, Julia, and ML baselines.

**Architecture:** One reference algorithm per benchmark in `benchmarks/<name>/main.li`; sibling dirs `impls/{cpp,rust,python,julia}/` with matched numerics; `bench.py` orchestrates build, run, verify, report.

**Design spec:** `docs/superpowers/specs/2026-05-14-li-language-design.md`

**Depends on:** Phase 3 (codegen incl. OpenMP), Phase 4 (stdlib); **SIMD + parallel for** for Tier 2; `tensor` for Tier 3 ML

---

## Principles

| Rule | Why |
|------|-----|
| **Same math, same constants** | Shared `params.toml` per benchmark (N, dt, epsilon, G, seed) |
| **Correctness gate first** | Energy drift, invariants, golden hashes before timing |
| **Document compiler flags** | `lic --release -O3 --target-cpu=native`; match in Clang `-O3 -march=native` |
| **No cheating** | Same FP type (`f64` default); NumPy may use BLAS for ML tier — label it |
| **Deterministic seeds** | RNG seed fixed; report platform + CPU model |
| **Provable-only li** | All li bench sources must `lic build` (Lean discharge) |

---

## Repository layout

```
benchmarks/
  harness/
    bench.py              # build all, run, CSV + markdown table
    verify.py             # invariant checks
    params_schema.toml
  tier0_correctness/      # type/shape compile-fail tests (no timing)
  tier1_micro/
    simd_dot/
    matmul_naive/
    matmul_blocked/
  tier2_physics/
    three_body/
    nbody_gravity/
    md_lennard_jones/
    harmonic_oscillator_chain/
    double_pendulum/
    heat_equation_2d/
    wave_equation_1d/
  tier3_ml/
    mlp_forward/
    mlp_train_step/
    conv2d_forward/
    softmax_cross_entropy/
    mnist_epoch_subset/
  tier4_integration/
    md_water_box_small/   # 256–512 particles, Ewald or cutoff
  impls/                  # optional: symlink or per-bench impls/
    cpp/
    rust/
    python/
    julia/
  results/
    YYYY-MM-DD_machine.md
    latest.csv
```

---

## Tier 0 — Correctness & type traps (no perf)

Compile-time and invariant tests proving the language catches science bugs.

| Test | What it proves |
|------|----------------|
| `bad_board_index.li` | `array[N,M]` literal OOB fails |
| `bad_tensor_contract.li` | shape mismatch fails (when `tensor` exists) |
| `md_energy.li` | single-step energy matches reference `f64` value |
| `three_body_invariants.li` | COM stationary; total momentum ~ 0 |

### Tier 0b — Shared-memory race rejects (v1, mandatory CI)

Exploit fixtures in `li-tests/race_shared_memory/` — each **must fail** `lic build`. Harness: `./li-tests/run_all.sh race_shared_memory`.

| Fixture | Must reject because |
|---------|-------------------|
| `shared_mut_write.li` | concurrent write same index |
| `overlap_par_slice.li` | overlapping `par_slice` |
| `missing_disjoint_clause.li` | `parallel for` without disjoint proof |
| `mut_capture_no_sync.li` | unsynchronized shared `var` capture |
| `borrow_mut_across_iters.li` | `borrow mut` + parallel overlap |
| `false_disjoint_proof.li` | spec lies about disjointness |

Positive: `good_disjoint_parallel.li` must **build**. Full test index: `li-tests/manifest.toml`.

---

## Tier 1 — Micro (Phase 5b, post-Tetris)

| Benchmark | Work | Default N | Metric |
|-----------|------|-----------|--------|
| `simd_dot` | `dot` on `simd[f64,8]` vs scalar | 10⁷ elements | ns/element |
| `matmul_naive` | ikj triple loop | 256³ | GFLOPS |
| `matmul_blocked` | cache-blocked + SIMD inner | 512³ | GFLOPS |
| `reduce_sum` | horizontal sum chain | 10⁸ | bandwidth GB/s |
| `lic_check` | typecheck harness sources | 10k LOC fixture | ms (not vs langs) |

**Cross-language:** C++ (Clang), Rust (`--release`), Julia, Python (NumPy for matmul only — column labeled `python+numpy`).

---

## Tier 2 — Physics simulations

### 2a `three_body` (v1 physics flagship)

- **Model:** 3 point masses, Newtonian gravity, pairwise forces
- **Integrator:** Velocity Verlet or Yoshida symplectic, `dt` fixed
- **Types:** `array[3, Vec2d]` or `object Body { pos: simd[f64,2], vel: ... }`
- **Steps:** 10⁶
- **Correctness:** energy error < 1e-4 relative over run; angular momentum drift report
- **Metric:** wall time; steps/sec

### 2b `nbody_gravity` (scalable N-body)

- **N:** 128, 512, 2048 bodies
- **Force:** O(N²) all-pairs (v1); Barnes–Hut in Tier 4
- **Same integrator** as three_body for fair scaling curve

### 2c `md_lennard_jones` (molecular dynamics)

- **Potential:** LJ 12-6 with cutoff `rc`
- **N:** 256 particles in periodic box (minimum image convention)
- **Integrator:** velocity Verlet
- **Steps:** 10⁴–10⁵
- **Correctness:** total energy drift < 0.1% over 10⁴ steps at `dt=0.004` (τ=1 LJ units)
- **Uses:** `array` or flat `tensor[(N,3), f64]`, SIMD force loop inner
- **Metric:** timesteps/sec; ns/day equivalent (report formula)

### 2d `harmonic_oscillator_chain`

- **N:** 64 masses, coupled springs
- **Analytical:** compare mode frequencies vs small-oscillation theory at t=0

### 2e `double_pendulum`

- **RK4** or symplectic; chaotic; compare trajectory hash at t=10s with reference impl
- **Metric:** steps/sec (chaos = sensitive; use hash not float bitwise)

### 2f `heat_equation_2d`

- **Grid:** 256×256, 5-point stencil, explicit Euler or Jacobi
- **Uses:** `array[256, array[256, f64]]`, SIMD row updates
- **Correctness:** L2 error vs analytical sin solution at t=T

### 2g `wave_equation_1d`

- **CFL** stable; pulse propagation; energy conservation

---

## Tier 3 — AI / ML benchmarks

Start after `tensor` + matmul blocked exist. No training framework in v1 — hand-rolled kernels first.

| Benchmark | Description | Sizes | Metric |
|-----------|-------------|-------|--------|
| `mlp_forward` | 2-layer ReLU MLP | batch=64, in=784, hidden=256, out=10 | forward pass/s |
| `mlp_train_step` | forward + backward + SGD | same | train step/s |
| `conv2d_forward` | naive conv | 32×32×3 → 16×16×16 kernel 3×3 | ops/s |
| `softmax_cross_entropy` | stable softmax + CE loss | batch=128, classes=1000 | ns/sample |
| `mnist_epoch_subset` | 1 epoch on 10k MNIST images | tiny MLP or CNN | epoch time |

**Cross-language baselines:**

| Label | Stack |
|-------|-------|
| `cpp_eigen` | Eigen3 matmul/MLP |
| `rust_ndarray` | `ndarray` + hand backward |
| `julia` | native loops + `LoopVectorization` optional column |
| `python_numpy` | NumPy (+ BLAS) |
| `python_numba` | Numba JIT optional column |
| `pytorch_cpu` | `torch` CPU, `torch.set_num_threads(1)` and `=N` columns |
| `jax_cpu` | optional; label XLA |

Report **single-thread** and **multi-thread** columns separately.

---

## Tier 4 — Integration (later)

| Benchmark | Notes |
|-----------|-------|
| `md_water_box_small` | 512 molecules, LJ + cutoff; closer to real MD |
| `barnes_hut_nbody` | N=16k |
| `lstm_forward` | small sequence model |
| `resnet18_infer_batch1` | compare PyTorch ONNX runtime |

---

## Harness behavior (`bench.py`)

```bash
# Correctness only
./benchmarks/harness/bench.py --tier 0 --ci

# Build li + refs, run tier 2, write results/latest.csv
./benchmarks/harness/bench.py --tier 2 --release --native --runs 5

# Full report markdown
./benchmarks/harness/bench.py --all --report results/$(date +%F)_$(uname -m).md
```

**CSV columns:** `benchmark, lang, variant, threads, metric, value, unit, git_sha, cpu_model, flags`

**Regression policy:** Tier 1–2 li within 1.2× of C++ on same machine or investigate before release tags.

---

## Shareable plots (X / social)

Plan: `docs/superpowers/plans/2026-05-14-plots-and-social.md`

```bash
./scripts/plot_shareables.sh
# → benchmarks/results/share/*.png
```

| Output | Purpose |
|--------|---------|
| `bench_speed_tier2.png` | Cross-language bar chart |
| `speedup_vs_cpp.png` | Li vs C++ ratio |
| `test_suite_pass_rate.png` | Conformance suite health |
| `test_suite_matrix.png` | pass/fail/skip heatmap |
| `ci_summary_card.png` | Single-image CI snapshot for X |

Plots use **16:9 dark theme**, Li branding, retina DPI. Regenerate after every benchmark sweep or before posting.

Harness: `benchmarks/harness/plot.py`, `li-tests/harness/plot_suites.py`

---

## Phase alignment

| When | Benchmarks unlocked |
|------|-------------------|
| Phase 5 (Tetris) | Tier 0 compile tests; harness skeleton |
| Phase 5b | Tier 1 micro + `three_body` scalar |
| Phase 3 codegen (SIMD + OpenMP) | `md_lennard_jones`, `heat_equation_2d`, blocked matmul — **parallel required** |
| Phase 3 ML std slice | Tier 3 MLP + MNIST subset |
| Post v1.0 | Tier 4; publish benchmark site |

---

## Master plan insertion

New phase **5b — Benchmarks & physics sims** after Tetris, before self-host:

| Exit gate |
|-----------|
| Tier 0 green in CI |
| `three_body` + `md_lennard_jones` correctness invariants pass |
| Published `results/latest.csv` vs C++/Rust/Julia/Python on one reference machine |
| At least one Tier 3 `mlp_forward` row before calling ML “started” |
| **`./scripts/plot_shareables.sh`** emits ≥4 share PNGs (see plots plan) |

Self-host remains **Phase 6** after language is live and benchmarks exist to catch regressions.

---

## Reference implementations checklist

For each Tier 2+ benchmark, before claiming speed:

- [ ] Shared `params.toml` checked into all impl folders
- [ ] Reference energy/trajectory hash documented in `README.md`
- [ ] C++ reference passes verify.py
- [x] Li reference passes `verify.py` and **`lic build`** (`tier0_correctness/*.li`; `test_harness_contract.py`; `bench.py --tier 0`)
- [x] Timing runs separated from verification runs (`benchmarks/harness/bench.py` verify gate vs timing sweep)
