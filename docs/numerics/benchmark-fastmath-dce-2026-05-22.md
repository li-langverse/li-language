# Whitepaper: Fast-math DCE in pure-Li benchmark verification (2026-05-22)

## Summary

While adding **result verification** for tier-1 micro-benchmarks, we found that a **pure-Li** `simd_dot` implementation could pass a naive “program runs and prints a number” check while doing **essentially no work**. The printed checksum was **`0`**, wall time was **~1 ms** for a nominal **10⁷-element** dot product, and LLVM had **eliminated the computation** under **`-O3 -ffast-math`**, not because we intended an optimized empty kernel.

This document is for internal review and external reporting: what happened, why our old checks were insufficient, and what we changed.

## Intended benchmark

**Name:** `simd_dot` (tier-1 micro).

**Normative semantics** (`benchmarks/tier1_micro/common/dot_core.c`):

1. `N = 10_000_000`
2. For each `i`: `a[i] = (i & 255) * 0.001`, `b[i] = ((i * 7) & 255) * 0.002` (float64)
3. `acc = Σ_i a[i] * b[i]`
4. Expose `acc` as the checksum (expected magnitude **~3.4×10⁵**, not zero)

**Performance intent:** measure throughput of Li’s `array @ array` / dot lowering at large `N`, comparable in *work* to the shared C kernel used by `cpp`/`rust`/`julia` labels.

## What we built first (pure-Li)

Early pure-Li drivers used **smaller tiles** (e.g. 800×`array[40000]`) or different init/reduction than `dot_core.c`. Result verify against native correctly **failed** (Li ≈ 64 vs native ≈ 340367) — good.

We then aligned init and total `N` with the C spec (tiled 10M, LUT / loop-based float init). **Numerical parity with native was still wrong in another way:**

| Build | Wall time (order of) | Checksum |
|--------|----------------------|----------|
| Native (`-O3 -ffast-math`, shared `dot_core.c`) | ~0.05–0.1 s | `340367.6243199876` |
| Pure-Li (`-O3 -ffast-math`, tiled loops + `li_rt_volatile_sink_f64`) | **~0.001 s** | **`0`** |

So Li vs native verify would have failed on checksum — but the important signal is **time + magnitude**: the Li binary was **three orders of magnitude too fast** and **summed to zero**.

## Root cause (compiler / flags, not “wrong formula”)

With **`--release -O3 -ffast-math -march=native`** (harness default for tier-1 Li):

1. LLVM treated the hot loop as **dead** or **foldable to zero** (unsafe math assumptions + no observable side effect on the reduction until the final sink).
2. **`li_rt_volatile_sink_f64(acc)`** on the **final** `acc` alone was **not enough** to keep the whole 10M-term reduction alive when intermediate array fills and partial sums were provably unused or folded.
3. This was **unverified** elimination — not forbidden DCE. **Allowed DCE** must still pass our spec, goldens, and timing guards (see policy below).

Isolated tests (e.g. `dot_lut_a(42)` returning `0.042`) still worked; the **full `main` + fast-math** path did not.

**Mitigation in tree today:** `simd_dot` Li links **`LI_EXTRA_C`** / `dot_core.c` (same kernel as native) until pure-Li codegen keeps observable work. **Matmul** pure-Li paths did not show the same total fold (large stack matrices retain memory effects).

## Why “compare to C++” was not enough

Our first verify pass was **Li checksum vs native `--verify`**. That catches **divergence** but not all failure modes:

| Failure mode | Li vs native only | Normative spec + guards |
|--------------|-------------------|-------------------------|
| Li wrong, native right | Fail | Fail |
| Both wrong the same way | **Pass** | Fail (vs spec / small golden) |
| Li DCE → 0, native correct | Fail | Fail (magnitude + time) |
| Li DCE → 0, native also broken | **Pass** | Fail |

**Python** (or any other runtime) is useful as a **second implementation** of the spec in `reference.py`, not as the definition of truth. Goal remains: **correct per spec, as fast as possible**; matching others is a sanity check.

## What we added (verification we own)

1. **`benchmarks/harness/reference.py`** — float64 reference loops mirroring `*_core.c`; **small-N exact** and **large-N** with tolerance.
2. **`verify_benchmark_results`** — native and Li must match spec; **`|result|` floors**; **min Li wall time** on pure-Li rows; optional `BENCH_VERIFY_TIMING`.
3. **`scripts/verify-math-physics-goldens.sh`** — library-scale small goldens (math `@` / `sum`, physics mini checksums).
4. **Cursor rule** `li-benchmark-correctness.mdc` — correct-first policy; **DCE allowed, verification required**.

## Recommendations (product / compiler)

1. **Pure-Li tier-1 dots:** require **observable reduction** in codegen (e.g. volatile store per tile, `noinline` kernel boundary, or `-ffast-math` off for verify builds) until a single sink at the end is proven sufficient.
2. **CI:** keep `./scripts/bench-verify-results.sh 1` and math/physics golden script on release paths.
3. **Reporting:** do not publish “pure_Li simd_dot” throughput until verify shows non-zero spec checksum at realistic wall time.

## References in repo

- Harness: `benchmarks/harness/bench.py`, `benchmarks/harness/reference.py`
- Incident driver history: `benchmarks/tier1_micro/simd_dot/li/main.li` (comment: shared kernel until pure-Li anti-DCE)
- Policy: `.cursor/rules/li-benchmark-correctness.mdc`, `benchmarks/results/README.md`
