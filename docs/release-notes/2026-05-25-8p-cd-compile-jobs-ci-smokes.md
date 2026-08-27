# 8p-c/d: ResourceOptions compile jobs + CI smokes

## Summary

`compiler/common` centralizes `ResourceOptions`; `lic build` finalizes flags, calls `note_compile_jobs_reserved`, and CI runs dedicated 8p parallel smoke scripts.

## Agent continuation

1. **Read:** `compiler/common/include/li/resource_options.hpp`, `compiler/lic/main.cpp`, `scripts/ci.sh` (8p parallel smokes).
2. **Run:** `./scripts/build.sh`; `./li-tests/tooling/ci_test_jobs_smoke.sh`; `./li-tests/tooling/resource_flags_smoke.sh`; `./li-tests/tooling/parallel_run_all_smoke.sh`.
3. **Next:** 8p-b `li_workspace_jobs` in `lic-workspace-build.sh`; wall-time SLO logging (8p-d).
4. **Blocked on:** none.

## Changed

| Path | Evidence |
|------|----------|
| `compiler/common/` | `ResourceOptions`, `apply_resource_flag`, `finalize_resource_options`, `note_compile_jobs_reserved` |
| `compiler/lic/main.cpp` | build/verify resource flag parsing (no local `setenv` for `--jobs`) |
| `compiler/codegen/include/li/platform.hpp` | `compile_jobs_from_options`, `max_memory_mb_from_env` via options |
| `scripts/lib/li-jobs.sh` | `li_workspace_jobs` |
| `scripts/ci.sh` | **8p parallel smokes** phase |
| `li-tests/tooling/ci_test_jobs_smoke.sh`, `resource_flags_smoke.sh`, `parallel_run_all_smoke.sh` | CI/local gates |
| `docs/superpowers/plans/2026-05-14-li-master-plan.md` | Phase **8p** partial 8p-a/b/c/d; PR [#186](https://github.com/li-langverse/lic/pull/186), [#200](https://github.com/li-langverse/lic/pull/200) |

## Not changed

- Parallel MIR/LLVM frontend passes (reserved job count only).
- `lic-workspace-build.sh` pool (8p-b).
- Vision-LLM `lic edit --patch=json`.
- Proof gates, `trusted.lean`, tier physics benches.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| Breaking | N/A — CLI flags preferred over env; one-time deprecation warnings |
| Security | N/A |
| Performance | Reserved `--jobs` + isolated `--build-dir` smokes; no new parallel codegen |
| Downstream | Agents should use smokes before full `scripts/ci.sh` |
