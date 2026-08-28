---
name: hpc-competitive-review
description: >-
  Competitive HPC review for Li — run tier benches, compare registry ecosystems,
  check competitor release notes manually, update registry last_reviewed, file gaps.
  Use when touching benchmarks, SIMD, parallel codegen, tier-2 physics, or perf claims.
---

# HPC competitive review

Use when editing `benchmarks/**`, SIMD/parallel codegen, or publishing performance numbers.

## Read first

1. `benchmarks/competitive/registry.toml`
2. `benchmarks/results/README.md` (shared_c_kernel vs pure_li)
3. `.cursor/rules/li-hpc-competitive.mdc`
4. `docs/benchmarks/competitive-landscape.md`

## Workflow

### 1. Registry + gate

```bash
./scripts/check-hpc-competitive.sh
./scripts/hpc-competitive-snapshot.sh   # optional; sets benchmarks/competitive/snapshots/latest.env
```

Fix registry schema errors before proceeding. Warnings about stale `last_reviewed` mean: review and bump the date.

### 2. Run benchmarks (local)

```bash
export LIC="$(./scripts/resolve-lic.sh)"
python3 benchmarks/harness/bench.py --tier 0          # strict stability (do not weaken)
python3 benchmarks/harness/bench.py --tier 12 --runs 5  # tier 1+2 timings → latest.csv
```

Compare `lang` columns (`cpp`, `rust`, `julia`, `li`) for each `compare` benchmark in the registry.

| Policy | Action |
|--------|--------|
| Tier 2 shared kernel | Li wall time should be ≤ **1.2×** cpp on same machine (advisory) |
| pure_li rows | Track separately; no parity gate until codegen matures |
| Missing lang column | Document why (Julia not installed) in PR; do not invent numbers |

### 3. Document results

- Append or refresh `benchmarks/results/YYYY-MM-DD_<machine>.md` if publishing externally.
- Run `./scripts/plot_shareables.sh` before social posts (see `benchmarks.mdc`).

### 4. Competitor upstream check (manual — no CI scraping)

For each `watch` ecosystem in the registry (Chapel, Kokkos, SYCL, Zig, NumPy):

1. Open `repo_url` release notes or changelog in a browser (human or agent with user approval).
2. Note 1–3 relevant changes (SIMD, PGAS, GPU offload, new std numerics).
3. If Li should respond: add **G-*** gap to master plan or `docs/benchmarks/competitive-landscape.md` — **no adoption without proof path**.
4. Set `last_reviewed` on reviewed `[[ecosystem]]` rows in `registry.toml`.

Stub for local notes (no network):

```bash
# Edit benchmarks/competitive/reviews/YYYY-MM-DD-notes.md by hand after reading upstream docs.
```

### 5. PR checklist

- [ ] Registry `last_reviewed` updated for touched ecosystems
- [ ] `check-hpc-competitive.sh` clean (warnings explained in PR if any)
- [ ] Perf claims cite `latest.csv` rows + honesty labels
- [ ] Parallel/OpenMP changes cite Lean / contract evidence
- [ ] Release notes if user-facing bench behavior changed (`write-li-release-notes`)

## Related

- `build-li-master-plan` — phase order; proof before perf
- `li-ecosystem-discipline` — functionality / security / performance gates
- [Benchmarks dashboard](https://li-langverse.github.io/benchmarks/)

## Do not

- Scrape GitHub APIs in CI for competitor versions (use manual review + optional env pins in snapshot script)
- Compare `main.li` stub wall time to full C++ MD without reading tier-2 PERF.md
- Ship `parallel for` without proof to match a competitor feature
