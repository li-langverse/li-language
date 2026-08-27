# Phase 7e-b (partial) — Tier 1 `matmul_blocked` pure-Li IKJ tiles

## Summary

`benchmarks/tier1_micro/matmul_blocked/li/main.li` runs **512× (64×64, block=16) IKJ** micro-tiles (≈512³ flops) with no `LI_EXTRA_C`; harness marks `matmul_blocked` as `li_pure=True`.

## Agent continuation

1. **Read** `benchmarks/tier1_micro/matmul_blocked/li/main.li` and `matmul_naive/li/main.li` for tile/rep patterns.
2. **Run** `./li-tests/run_all.sh` and `python3 benchmarks/harness/bench.py --tier 1` (expect `matmul_blocked verify ok (pure Li)`).
3. **Then** merge stack **#138→#144→this**; benchmarks dashboard ingest after tag; optional full 512×512 when stack arrays scale.
4. **Blocked on** slice syntax for true 512³ single allocation (v1 uses tiled equivalence).

## Changed

| Path | What |
|------|------|
| `benchmarks/tier1_micro/matmul_blocked/li/main.li` | IKJ blocked loops + `C[i][j] = C[i][j] + aik * B[k][j]` |
| `benchmarks/harness/bench.py` | `li_pure=True` for `matmul_blocked` |
| `benchmarks/results/README.md` | `pure_li` lists `matmul_blocked` |

## Not changed

- Full 512×512 Li arrays on stack (still 64×64 tiles × 512 reps).
- `simd_dot` / `ArrayBinOpF64` SIMD codegen.
- Compiler MIR (uses existing 2d load/store + binops).
- benchmarks org `catalog.toml` ingest.

## Breaking

N/A — Li column label `pure lic`; cpp/rust/julia still use shared C kernel.

## Security

N/A.

## Performance

Smoke: pure-Li ~0.013s vs native ~0.011s on tier-1 verify (not parity-gated). GFLOPS uses `flops_per_run=2×512³`.

## Downstream

- Master plan **7e-b** — `matmul_blocked` partial alongside `matmul_naive`.
