# Phase 7: Native HPC (SIMD + parallel for)

> **Depends on:** Phases 3–5b (MIR/LLVM, benchmarks harness)  
> **Blocks:** Pure-Li Tier 2 perf tables, OpenMP scaling columns

**Proof gaps (Doc-c):** [G-par](../../verification/provability-gaps.md#g-par) · [G-dec](../../verification/provability-gaps.md#g-dec) · [G-math](../../verification/provability-gaps.md#g-math) · [still open](../../verification/provability-gaps.md#still-open-report-every-session)

**Goal:** Built-in `simd[T, N]` and proved `parallel for` without user-level parallel/math libraries. Toolchain links LLVM + libomp only.

## 7a — SIMD vertical slice

| Task | Exit |
|------|------|
| `simd[T,N]` in typechecker (`TyKind::Simd`) | `li-tests/simd/` pass |
| MIR: splat, binop, horizontal sum | LLVM `<N x double>` |
| `simd_dot` benchmark pure Li | `li_pure=True` in harness |

## 7b — `parallel for` + OpenMP

| Task | Exit |
|------|------|
| `Stmt::ParallelFor` AST + parser | Parses exploit fixtures |
| Outlined par body + `li_omp_parallel_for` in `runtime/li_rt.c` | `-fopenmp` link |
| Replace `policy.cpp` string hacks with structured overlap check (keep fixtures) | `race_shared_memory` green |
| `lic build --threads=N` / `LI_OMP_THREADS` | CSV threads column |

## 7c — Benchmark truthfulness

| Task | Exit |
|------|------|
| `md_lennard_jones` pure Li driver | No `LI_EXTRA_C` for li label |
| Tier 2 verify checksum | `bench.py` smoke |

## 7d — Execution decorators (decorator-first HPC)

> **Depends on:** **2g** (`def`), **7a** (SIMD), **7b** (`parallel for` + structured disjoint)  
> **Plan:** [.cursor/plans/li_execution_decorators_7c6e3b42.plan.md](../../../.cursor/plans/li_execution_decorators_7c6e3b42.plan.md)  
> **Spec (to land):** `docs/superpowers/specs/2026-05-16-li-execution-decorators.md`

**Goal:** Primary surface for parallelism, vectorization, and device placement is **stackable `@` decorators** on `def` and on `for`/`while` — elaborating to the same proved cores as keywords (`parallel for`, `simd`, future `gpu proc`).

| Sub | Task | Exit |
|-----|------|------|
| **7d-a** | Lexer `@`, decorator lists on `def`/`for`/`while`, AST attrs | Parse tests — **done** |
| **7d-e (partial)** | Policy: `reserved_name`, typosquat, `parallel_requires_disjoint` | `decorator_exploits/` CI |
| **7d-b** | Elaboration → `ParallelFor` / `simd` / host placement MIR tags | `li-tests/decorators/` positive — **partial:** `@vectorized(lanes=4)`, `@no_vectorize` |
| **7d-c** | Structured `disjoint=`; scoped `@vectorized` on `for` (`ArraySimdScope`) | `vectorized_for_scope_ok.li` — **partial:** disjoint= still **G-par** open |
| **7d-d** | `std/execution/decorators.li` + `docs/language/decorators.md` | Handbook + gallery |
| **7d-e** | `decorator def` with **strict naming** (package prefix, typosquat ban), expansion whitelist | `li-tests/decorator_exploits/` all **fail** except control; CI on every PR |

**Policy (binding):**

- Stdlib names (`parallel`, `vectorized`, `async`, `cpu`, `gpu`, `tpu`, `user_defined`, …) are **reserved** — no Python-style shadowing via import or user `decorator def`.
- User decorators use **their own** multi-segment names (`li_math_tiled_parallel` in package `li-math`).
- **`@user_defined(...)`** is the stdlib hook for custom chips, not a user-chosen decorator name.

## CI (parallel track)

- Windows: build + `run_all.sh --ci` + security
- Fuzz: daily `fuzz.yml`, `merge_fuzz_corpus.sh`, corpus artifact + optional bot PR
- `scripts/ci.sh`: log `race_shared_memory` and **`decorator_exploits`** explicitly
- TSan nightly (post-7b): optional `memory.yml` job

## Exit gate (phase complete)

**7a–7c (Phase 7 core):**

- [x] `./li-tests/run_all.sh simd race_shared_memory`
- [x] `bench.py --tier 0` in CI; tier 1/2 perf runs advisory via `bench.py`
- [x] Fuzz workflow present (`.github/workflows/fuzz.yml`); `scripts/export-fuzz-status.sh`

**7d (decorators — can ship after 7b; recommended before calling HPC “done” for users):**

- [x] `./li-tests/run_all.sh decorators decorator_exploits`
- [ ] Tier 2 MD example uses `@cpu` `@parallel` `@vectorized` on `def` (elaborates to same MIR as keywords)
- [x] Fuzz corpus includes `@` decorator stacks and reserved-name parse seeds (`compiler/fuzz/corpus/seed_decorators`)

**7e (mathematical surface — user writes formulas, not `simd(...)`):**

> **Plan:** [2026-05-16-li-math-linalg-surface.md](2026-05-16-li-math-linalg-surface.md)

| Sub | Task | Exit |
|-----|------|------|
| **7e-a** | Lower `*`, `+`, `dot`, `sum` to 7a SIMD MIR | **partial:** `simd_dot` pure-Li `a @ b` (#148) |
| **7e-b** | Lower `A @ B` for Tier 1 matmul benches | **partial:** `matmul_naive` / `matmul_blocked` pure-Li; **≤1.2× C++** advisory |
| **7e-c** | `docs/language/linear-algebra.md`, `docs/guide/math-hpc-examples.md` | **done** on `main` |
| **7e-d/e** | `ArrayDotF64` / `ArrayBinOpF64` gather SIMD | **partial** on `main` (#148) |
| **2f / P-linalg** | Contract corpus for dot/sum/matmul entry | **partial:** #151 closed + loop open — **G-math**, **G-lean** |

- [x] `./li-tests/run_all.sh math_linalg`
- [x] Tier 1 Li sources: math notation only (`a @ b`, `C = A @ B` — no user `__li_simd_*`)
- [ ] Tier 1 perf: Li within **1.2×** C++ on same machine (investigate reds on dashboard)
