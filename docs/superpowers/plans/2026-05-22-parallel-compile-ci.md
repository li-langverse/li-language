# Phase 8p — Parallel compile & CI throughput

**Master plan:** [2026-05-14-li-master-plan.md](2026-05-14-li-master-plan.md) § Phase 8p

**Problem:** Sequential `lic build` in `li-tests/run_all.sh` dominates local CI wall time (~4–8 min for manifest alone; full `local-ci.sh` often 10+ min).

## Exit gate (phase complete)

On a Linux devbox with **≥8 logical cores** and LLVM 22 + Lean installed:

| Check | Target |
|-------|--------|
| `LI_TEST_JOBS=8 ./li-tests/run_all.sh` | 196/196 pass, exit 0 |
| Same with `LI_TEST_JOBS=1` | Identical pass/fail (deterministic) |
| Wall time | `run_all.sh` with `-j8` ≤ **50%** of sequential (logged once in baseline) |
| `HTTPD_SKIP_LI_ROUTING_BIN=1 ./scripts/local-ci.sh` | exit 0 |

## Implementation queue (order matters)

### 8p-a — Parallel test orchestration (highest ROI)

**Files:** `li-tests/run_all.sh`, `scripts/lib/li-ui.sh` (optional progress), `docs/guide/getting-started-tools.md`

1. Add `LI_TEST_JOBS` (default: `nproc` / `sysctl hw.ncpu`) and optional `-j N` CLI on `run_all.sh`.
2. For each manifest row, run `run_one` in a worker subprocess with:
   - `LI_BUILD_DIR="$ROOT/../build/li-test-$$-$RANDOM"` or similar isolated tree, **or**
   - `lic build --no-lean-verify` only for outcomes that do not touch shared AutoVC (narrower v1).
3. Preferred v1: **isolated `LI_BUILD_DIR` per job** + export in `lic` if env already supported; else temp dir + copy semantics doc.
4. Aggregate pass/fail/skip; preserve current footer format.
5. Document `LI_TEST_JOBS=1` for golden CI logs.

**Risk:** Shared `build/generated/AutoVC.lean` — must not run two Lean typechecks on the same path concurrently.

### 8p-b — Workspace & harness

**Files:** `scripts/lic-workspace-build.sh`, `benchmarks/harness/bench.py` (tier 0: avoid double full `run_all` when manifest already ran)

1. `xargs -P "$LI_TEST_JOBS"` or GNU `parallel` over workspace members.
2. bench tier 0: call `verify.py` only if `run_all` not already run in same CI phase (ci.sh ordering).

### 8p-c — Compiler `--jobs`

**Files:** `compiler/lic/main.cpp`, `compiler/codegen/include/li/platform.hpp`, language design § parallelism

1. Wire `LI_COMPILE_JOBS` / `--jobs=N` to any embarrassingly parallel MIR/LLVM passes (start with documented no-op → real work in follow-up PRs).
2. Do not confuse with `lic build --threads=N` (OpenMP **runtime**).

### 8p-d — CI defaults & observability

**Files:** `scripts/ci.sh`, `scripts/local-ci.sh`, `docs/ecosystem/lic-ecosystem-baseline.md`, agent skill `run-local-ci-gha-quota`

1. Export `LI_TEST_JOBS` in CI when not set.
2. Log `wall_s` for `run_all` + full local-ci in baseline after green run.
3. GHA: document in workflow comments (no quota burn on matrix expansion — single job, internal `-j`).

## Environment variables (canonical)

| Variable | Scope | Default |
|----------|--------|---------|
| `LI_BUILD_JOBS` | CMake/Ninja C++ build | host cores |
| `LI_TEST_JOBS` | `run_all.sh`, workspace smoke | host cores (8p-a); until shipped: **1** |
| `LI_COMPILE_JOBS` | `lic build` frontend (8p-c) | host cores when wired |
| `lic build --threads=N` | **Runtime** OpenMP only | not compile parallelism |

## Out of scope (v1)

- Distributed / remote compilation (Bazel remote, sccache cluster).
- Parallelizing Lean proof search inside a single VC (future proof cache).

## Agent continuation

1. Read master plan § 8p.
2. Implement **8p-a** first; verify with `LI_TEST_JOBS=8` and `=1`.
3. Update ecosystem baseline wall-time row when gate met.
4. Close master plan tracker checkbox for **8p** when all sub-phases exit gates pass.
