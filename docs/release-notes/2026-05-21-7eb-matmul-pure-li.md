# Phase 7e-b (partial) — Tier 1 `matmul_naive` pure-Li `@`

## Summary

`benchmarks/tier1_micro/matmul_naive/li/main.li` now runs **64× (64×64) `@` tiles** (≈256³ flops) with no `LI_EXTRA_C`; harness marks `matmul_naive` as `li_pure=True`.

## Agent continuation

1. **Read** `docs/release-notes/2026-05-21-oop-2i-matrix-matmul.md` (requires **2i-c** on compiler branch).
2. **Run** `cmake --build build && ./li-tests/run_all.sh` and `python3 benchmarks/harness/bench.py --tier 1` (or `verify_checksum` for `matmul_naive`).
3. **Then** **7e-a** SIMD lowering on matmul loops, or **matmul_blocked** pure-Li blocked tiles (needs slice/`@` on sub-blocks).
4. **Blocked on** merging **#138** (2i-c) before this bench is meaningful on `main`.

## Changed

| Path | What |
|------|------|
| `benchmarks/tier1_micro/matmul_naive/li/main.li` | `matmul_tile_f64` + `C = A @ B`; no `__li_simd_*` |
| `benchmarks/harness/bench.py` | `li_pure=True` for `matmul_naive` |
| `benchmarks/results/README.md` | `pure_li` column lists matmul |

## Not changed

- `matmul_blocked` — still `LI_EXTRA_C` (512³ stack / blocked slices).
- `simd_dot` — still uses `__li_simd_*` (7e-a target).
- Compiler **2i-c** — landed on sibling branch `cursor/oop-2i-matrix-matmul-57b4` / PR #138.
- Benchmarks org `catalog.toml` ingest (dashboard) — run ingest after lic tag if needed.

## Breaking

N/A — Li column variant label becomes `pure lic` in CSV; shared C kernel column unchanged for `cpp`/`rust`/`julia`.

## Security

N/A.

## Performance

Pure-Li wall time >> native 256³ C kernel on smoke hardware (expected until **7e-a** SIMD). GFLOPS uses same `flops_per_run=2×256³` as native reference.

## Downstream

- Master plan **7e-b** checkbox — `matmul_naive` partial.
- **benchmarks** dashboard ingest when lic release includes this commit.
