# 8p-a: parallel li-tests orchestration

## Summary

`li-tests/run_all.sh` runs manifest rows concurrently via `LI_TEST_JOBS` / `-j N`, with per-worker `LI_BUILD_DIR` trees; `lic` honors `LI_BUILD_DIR` for AutoVC emission and open-goal checks.

## Agent continuation

1. **Read:** `scripts/lib/li-jobs.sh`, `li-tests/run_all.sh`, `compiler/codegen/include/li/platform.hpp`, `compiler/lic/main.cpp` (`repo_build_path`).
2. **Run:** `./scripts/build.sh`; `LI_TEST_JOBS=4 ./li-tests/run_all.sh math_syntax`; `LI_TEST_JOBS=1 ./li-tests/run_all.sh math_syntax`.
3. **Next:** 8p-b parallel `lic-workspace-build.sh`; 8p-c wire `LI_COMPILE_JOBS`.
4. **Blocked on:** wall-time SLO logging (8p-d).

## Changed

| Path | Evidence |
|------|----------|
| `scripts/lib/li-jobs.sh` | `li_test_jobs`, `li_host_jobs` |
| `li-tests/run_all.sh` | `-j`, job pool, `build/li-test-<id>/` |
| `compiler/codegen/include/li/platform.hpp` | `repo_build_prefix`, `repo_build_path` |
| `compiler/lic/main.cpp` | AutoVC/vcs under active build dir |
| `scripts/ci.sh` | `export LI_TEST_JOBS` when `CI=true` |
| `docs/superpowers/plans/2026-05-14-li-master-plan.md` | Phase **8p** tracker (8p-a partial) |

## Not changed

- `lic build --jobs=N` still env-only (8p-c).
- Workspace member builds still sequential (8p-b).
- Vision-LLM `lic edit --patch=json`.
- Phase 2i full rank broadcast; 7d MIR proc tags.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A — default local `LI_TEST_JOBS=1` |
| **Security** | N/A |
| **Performance** | Manifest wall time scales ~∝ tests/cores when `LI_TEST_JOBS>1` |
| **Downstream** | N/A |
