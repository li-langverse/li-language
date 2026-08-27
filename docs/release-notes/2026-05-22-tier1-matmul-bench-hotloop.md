# Tier-1 matmul benches: hoist init out of hot loop

## Summary

`matmul_naive` and `matmul_blocked` Li drivers initialize `A`/`B` once per run; the timed loop is only `C = A @ B` (64 and 512 tiles), matching C++ kernel structure (setup outside hot path).

## Agent continuation

1. **Run:** `python3 benchmarks/harness/bench.py --tier 1`; `./scripts/check-tier1-li-vs-cpp.sh`.
2. **Next:** 512×512 single `@` when shape surface allows; blocked IKJ codegen in Li (not scalar loops).
3. **Blocked on:** **P-float** sqrt proof.

## Changed

| Path | Evidence |
|------|----------|
| `benchmarks/tier1_micro/matmul_naive/li/main.li` | init once; 64× `@` |
| `benchmarks/tier1_micro/matmul_blocked/li/main.li` | init once; 512× `@` |

## Not changed

- C++ reference kernels (still one 256³ / 512³ call per process).
- `ArrayMatMul2DF64` codegen.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A — bench harness only |
| **Security** | N/A |
| **Performance** | Li/c++ ratio should improve vs prior init-in-loop drivers |
| **Downstream** | Re-run benchmarks dashboard ingest after local CSV refresh |
