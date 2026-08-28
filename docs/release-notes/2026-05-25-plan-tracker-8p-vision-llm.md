# Plan tracker: 8p-a parallel li-tests + Vision-LLM agent manifest slice

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PH / REQ:** Phase **8p-a**, **Vision-LLM** (master plan tracker)

---

## Summary

Ships parallel `li-tests` orchestration with isolated `LI_BUILD_DIR` per worker (8p-a) and a compact JSON export of the test manifest for agents (Vision-LLM partial).

## Agent continuation

1. Read: `docs/superpowers/plans/2026-05-22-parallel-compile-ci.md` § 8p-b–d; `docs/superpowers/plans/2026-05-14-li-master-plan.md` tracker.
2. Run: `./scripts/build.sh && ./li-tests/tooling/run_all_parallel_smoke.sh && LI_TEST_JOBS=2 ./li-tests/run_all.sh math_linalg && ./li-tests/tooling/agent_manifest_smoke.sh`
3. Then: 8p-b parallel workspace (`lic-workspace-build.sh`); wire `lic build --jobs` to MIR passes (8p-c).
4. Blocked on: full 8p wall-time SLO measurement on 8-core Linux (log to `docs/ecosystem/lic-ecosystem-baseline.md`).

## Changed

| Area | What | Evidence |
|------|------|----------|
| 8p-a | `LI_TEST_JOBS`, `-j N`, isolated `LI_BUILD_DIR` | `li-tests/run_all.sh`, `scripts/lib/li-jobs.sh` |
| Compiler | AutoVC/vcs under `LI_BUILD_DIR` | `compiler/codegen/include/li/platform.hpp`, `compiler/lic/main.cpp` |
| Vision-LLM | Agent manifest JSON slice | `scripts/export-li-tests-agent-slice.sh`, `li-tests/agent-manifest.json` (generated in CI) |
| CI | Smokes + parallel default in `ci.sh` | `li-tests/tooling/run_all_parallel_smoke.sh`, `agent_manifest_smoke.sh` |
| Tracker | Honest partial rows | `docs/superpowers/plans/2026-05-14-li-master-plan.md` |

## Not changed

- Phase **2i**, **7d**, **7e** implementation — tracker text only elsewhere.
- `provability-gaps.md` body — no edits.
- Phase sub-plan checkboxes — not bulk-edited.
- `lic build --jobs` frontend parallelism (8p-c) — flag still reserved.
- Workspace parallel builds (8p-b).

## Breaking changes

None.

## Security

N/A — test orchestration only; no trusted surface change.

## Performance

Parallel `run_all.sh` reduces wall time when `LI_TEST_JOBS>1`; default remains **1** locally, host cores when `CI=true`. No bench ratio claims in this PR.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / lis | N/A |

## CHANGELOG entry

```markdown
### Added
- **8p-a (partial):** parallel `li-tests/run_all.sh` with `LI_TEST_JOBS` / `-j` and per-worker `LI_BUILD_DIR` — `docs/release-notes/2026-05-25-plan-tracker-8p-vision-llm.md`.
- **Vision-LLM (partial):** `li-tests/agent-manifest.json` export for agents — `scripts/export-li-tests-agent-slice.sh`.
```
