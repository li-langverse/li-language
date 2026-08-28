# 7d/7e: def @parallel disjoint + tier-1 bench build

## Summary

`@parallel(disjoint=…)` on a `def` now satisfies inner `parallel for` disjoint policy; tier-1 `bench.py` passes `--allow-open-vc` so perf kernels build under default Lean gate.

## Agent continuation

1. **Run:** `python3 benchmarks/harness/bench.py --tier 1`; `./scripts/check-tier1-li-vs-cpp.sh`.
2. **Next:** MIR tags from proc decorators; full rank broadcast; close bench VCs where possible.
3. **Blocked on:** libm sqrt ULP lemmas (**P-float**).

## Changed

| Path | Evidence |
|------|----------|
| `compiler/types/policy_module.cpp` | proc-level `disjoint=` inherits to nested `parallel for` |
| `li-tests/decorators/parallel_def_disjoint_inherit.li` | compile without per-loop `requires disjoint_*` |
| `benchmarks/harness/bench.py` | `--allow-open-vc` on Li tier-1 builds |

## Not changed

- `sqrt_open_bound` proof closure.
- NumPy full broadcast.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A — relaxes policy when def already has `@parallel(disjoint=…)` |
| **Security** | N/A |
| **Performance** | Enables tier-1 CSV refresh in CI |
| **Downstream** | benchmarks ingest after CSV commit |
