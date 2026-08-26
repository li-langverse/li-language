# Li ecosystem baseline — phase 0 (2026-05-22)

**Purpose:** Clean starting point for the **lic ecosystem gap loop** (scientific / engineering / gaming packages). **Out of scope:** li-httpd / webserver work (separate agent on `cursor/httpd-plan-loop-54aa`).

**Plan:** [algorithms-and-libraries-plan.md](algorithms-and-libraries-plan.md) §7–§12 · **Proof honesty:** [provability-gaps.md](../verification/provability-gaps.md)

## Branch / PR target

| Item | Value |
|------|--------|
| Implementation branch | **New branch from `main`** after [#174](https://github.com/li-langverse/lic/pull/174) merge (phase 0 landed) |
| Phase 0 on `main` | [#174](https://github.com/li-langverse/lic/pull/174) — algorithms plan, LLVM 22, Lean CI, manifest honesty, phase **8p** |
| Loop driver | `scripts/lic-ecosystem-plan-loop.py` (phase 1; start on fresh branch) |

## CI while GHA quota exhausted

- **Do not** treat missing GitHub checks as green.
- Run **`./scripts/local-ci.sh`** on this branch before merge (see skill `run-local-ci-gha-quota`).
- Record: `benchmarks/data/latest/local-ci-results.json` when validating a PR number.

## 8p throughput observability (Linux devbox, LLVM 22 + Lean)

| Metric | `run_all -j1` | `run_all -j8 --max-memory=8192` | Notes |
|--------|---------------|-----------------------------------|--------|
| **wall_s** (full manifest) | not measured | not measured | Re-measure after [#205](https://github.com/li-langverse/lic/pull/205) + integrator PR; target ≤50% vs `-j1` ([8p plan](../superpowers/plans/2026-05-22-parallel-compile-ci.md)) |
| **peak_rss** (full manifest) | not measured | not measured | Use `/usr/bin/time -l ./li-tests/run_all.sh …` or `scripts/profile-memory.sh` |

`scripts/ci.sh` passes explicit `-j8 --max-memory=8192` to manifest `run_all` phases (no `LI_TEST_JOBS` export).

## Stale / parallel work — do not conflate

| Track | Branch | Owner |
|-------|--------|--------|
| **httpd M1** | `cursor/httpd-plan-loop-54aa` (#173) | Webserver agent |
| **httpd loop infra** | — | **Merged** [#172](https://github.com/li-langverse/lic/pull/172) to `main` |
| **Ecosystem gaps** | `cursor/lic-ecosystem-plan-loop-54aa` | This loop |

Do not edit `runtime/li_rt_httpd.c`, `scripts/httpd-plan-loop.py`, or httpd plan YAML on the ecosystem branch unless fixing a **shared** gate (build/CI only).

## Phase 0 checklist

- [x] `algorithms-and-libraries-plan.md` on ecosystem branch
- [x] This baseline doc
- [x] Agent skill `run-local-ci-gha-quota` in roadmap agent-kit (+ `li-local-ci`)
- [x] Local CI green on devbox (`li-tests` **196/0**; tier-0 `verify.py` uses `--allow-open-vc` to match manifest honesty)
- [x] **8p-d** CI integrator — `scripts/ci.sh` uses `run_all.sh -j8 --max-memory=8192` (stacked on [#205](https://github.com/li-langverse/lic/pull/205))
- [ ] **8p** wall-time SLO — log `wall_s` / `peak_rss` in table above after green `local-ci.sh`
- [x] LLVM 22 + Lean 4 on devbox (`lake build` in `docs/semantics` ok)
- [x] PR [#174](https://github.com/li-langverse/lic/pull/174) **merged** to `main`
- **Run CI:** `HTTPD_SKIP_LI_ROUTING_BIN=1 ./scripts/local-ci.sh` if port bind conflicts with another agent
- [ ] PR opened; human `merge-approved` after review
- [ ] Phase 1: `lic-ecosystem-plan-loop.py` + `data/lic-ecosystem-plan-loop/state.json`

## Default queue head (phase 1+)

Per §7.7 — first implementation slices after gates green:

1. **P0** Wave A slices (2e/2f) — bounded, no domain kernel scale-up
2. **P0** AL-11 — `li-math` quaternions + `Mat4`; wire `li-scene`
3. **P1** AL-10 — `packages/linalg` scaffold + `math_linalg` tests

## Agent continuation

1. Read §12 handoff in [algorithms-and-libraries-plan.md](algorithms-and-libraries-plan.md).
2. Skill **`run-local-ci-gha-quota`** — run local CI before claiming done.
3. Work only on **`cursor/lic-ecosystem-plan-loop-54aa`**; PR-only; no self-merge.
4. Skip httpd todos and httpd-only files.
