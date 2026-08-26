# Release notes: 2026-05-25 — plan-checkbox-audit-wave

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** (feat/plan-checkboxes-wave)  
**PH / REQ:** PH-doc hygiene (plan sync)  
**Author:** agent

---

## Summary (one sentence)

Marked 66 plan-file checkboxes `[x]` where master-plan phase trackers, C++ compiler paths, or `li-tests` evidence already exist; left 24 open (partial phases, Tetris sub-features, bench verify gaps, plots, perf gates).

## Agent continuation (required)

1. Read: `benchmarks/data/latest/plan-completion-audit.json` with `LIC_ROOT` set to lic checkout; `docs/superpowers/plans/2026-05-14-li-master-plan.md` partial rows (2i, 7d, 7e, 8p, Vision-LLM).
2. Run: `LIC_ROOT=$PWD python3 ../benchmarks/scripts/plan-completion-audit.py` after merge.
3. Then: optional benchmarks follow-up — teach `plan-completion-audit.py` to skip Phase 0 bootstrap boxes when master Phase 0 is `[x]`; implement bench `verify.py` C++ path before closing benchmarks checklist rows.
4. Blocked on: human merge; perf/plots rows need dashboard or `./scripts/plot_shareables.sh` green.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| Plans | Phases 0–4 exit gates → `[x]` with C++ paths noted | `compiler/types/`, `compiler/mir/`, `compiler/codegen/`, `runtime/li_rt.c` |
| Plans | Phase 5 Tetris partial sync | `examples/tetris/`, `li-tests/tetris/board_oob.li` |
| Plans | Pkg + governance exit gates | `scripts/li-new-package`, `docs/ecosystem/official-packages.md`, `packages/*/PUBLISH.md` |
| Plans | P-linalg loop witness | `li-tests/contracts_verify/linalg_dot4_int_loop_open.li` |
| Plans | Bench harness timing gate | `benchmarks/harness/bench.py` verify vs timing split |

## Not changed (scope fence)

- `docs/verification/provability-gaps.md` — **not** edited (other agent)
- Compiler C++ implementation — **not** changed
- `li-tests` harness implementation — **not** changed
- Master plan partial phase tracker rows (2i, 7d, 7e, 8p, Vision-LLM) — remain `[ ]`

## Breaking changes

None.

## Security

N/A — documentation-only sync.

## Performance

N/A — no codegen or bench threshold changes.

## Downstream

| Repo | Action |
|------|--------|
| benchmarks | Optional: `plan-completion-audit.py` bootstrap skip when Phase 0 `[x]` |
| lip / lit / lis | N/A |

## CHANGELOG entry (paste into Unreleased)

```markdown
### Changed
- Plan checkbox audit wave — sync phase plan exit gates with shipped compiler evidence ([#PR](URL))
```
