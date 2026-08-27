# Benchmark results

## Columns (read `latest.csv` and tier reports)

| Column | Meaning |
|--------|---------|
| **shared_c_kernel** | Li driver links the same C/C++ kernel as `cpp`/`rust`/`julia` via `LI_EXTRA_C` (wall time ≈ native reference). |
| **pure_li** | Logic compiled from `.li` only (`horner_pure_li`, `simd_dot` via `a @ b`, `matmul_naive` / `matmul_blocked` math loops). Often slower until **7e** codegen matures. |

Do not compare `main.li` MD stub wall time to full `md_main.c` without reading [tier2_physics/md_lennard_jones/PERF.md](../tier2_physics/md_lennard_jones/PERF.md).

## Competitive columns policy

Tracked ecosystems live in `benchmarks/competitive/registry.toml` (`cpp`, `rust`, `julia`, `li` in `latest.csv` today). **Watch-only** competitors (Chapel, Kokkos, SYCL, Zig) have no CSV column until a harness driver exists.

- Before publishing cross-language numbers: run `./scripts/check-hpc-competitive.sh` and skill `hpc-competitive-review`.
- **shared_c_kernel** rows compare wall time with the same C core; **pure_li** rows are not parity-gated against `cpp`.
- No performance claims without a reproduced CSV row on documented flags/hardware.
- See `docs/benchmarks/competitive-landscape.md`.

## Policy

- **Tier 0 (stability):** strict invariants must pass for all backends.
- **Result verify (tier 1/2):** before timings (or standalone), each binary is checked against the **normative kernel spec** in `benchmarks/harness/reference.py` (float64 loops from `common/*_core.c`, plus **small-N exact** cases). Then **Li vs native** for consistency. **DCE is allowed**; harness verifies (spec, small-N goldens, `|result|` floor, min Li wall time on pure-Li rows). Disable spec checks with `BENCH_VERIFY_REFERENCE=0`. Optional: `BENCH_VERIFY_TIMING=1`.
- **Tier 2 (physics):** shared-kernel target **≤ 1.2×** C++ on reference hardware (advisory MSD rows until harness fixed).
- **Tier 1 (micro):** pure-Li rows tracked separately; not gated at parity until **2i/7e** complete.

## Machine-readable summaries (`li_sim_summary_v1`)

Per-bench summaries (not CSV): pretty JSON, minified JSON, or YAML — same schema.

```bash
python3 benchmarks/harness/verify.py --write-summary --summary-format json
python3 benchmarks/harness/verify.py --write-summary --summary-format json_min
./li-tests/tooling/sim_li_run_summary.sh   # Li composable → benchmarks/results/li_runs/
./scripts/validate-sim-summary.sh
```

See [sim-output-contract.md](../../docs/ecosystem/sim-output-contract.md). Detail: `summary` | `fields` | `debug` | `repro`. Format: `LI_SIM_SUMMARY_FORMAT` or `--summary-format`.

## Studio UI/UX (PH-UX)

Registry: `benchmarks/competitive/studio-ui.toml`. Harness writes:

- `data/studio-ui-ux-plan-loop/latest-bench.json` (plan loop)
- `benchmarks/results/bench-studio-viewport-perf.json` (competitive snapshot; regenerated, not committed)

```bash
./scripts/bench-studio-viewport-perf.sh
python3 scripts/studio-ui-ux-verify-bench-registry.py
```

## Regenerate

```bash
python3 benchmarks/harness/bench.py --tier 0
./scripts/bench-verify-results.sh 1    # results only, tier 1
python3 benchmarks/harness/bench.py --tier 12 --runs 5
```
