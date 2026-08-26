---
name: run-goal-directed-plan-loop
description: >-
  Run the lic httpd goal-directed plan loop (code_implementer, overnight batches,
  until 08:00 local, push recovery, local Pages). Use for httpd-plan-loop.py,
  autonomous httpd todos, or continuing agent batches until a morning deadline.
---

# Goal-directed plan loop (lic / httpd)

Canonical **httpd** wiring. Full pattern (new plans, li-cursor-agents):  
`li-cursor-agents/.cursor/skills/run-goal-directed-plan-loop/SKILL.md`.

## Commands

```bash
cd lic
export LI_CURSOR_AGENTS_ROOT=../li-cursor-agents
export BENCHMARKS_ROOT=../benchmarks
export HTTPD_PLAN_PR_BRANCH=cursor/httpd-plan-continue   # feature branch

./scripts/httpd-plan-loop.py --dry-run
./scripts/httpd-plan-loop.py --once
HTTPD_PLAN_TZ=Europe/Berlin ./scripts/httpd-plan-overnight.sh   # --max 30, then until 08:00 in TZ
./scripts/httpd-plan-until-deadline.sh       # extension only (waits for in-flight loop)
```

**Deadline math:** `until-deadline` uses `MIN_PER_ITER` (default 12m) and remaining wall time in `TZ` to pick each batch `--max`. From 21:34 → 08:00 next day ≈ 10.5h → ~52 iterations at 12m (capped by `HTTPD_PLAN_BATCH_CAP`).

Secrets: `~/Documents/Cursor/.env` → `CURSOR_API_KEY`, `GH_TOKEN`.

## Defaults (overnight)

| Setting | Value |
|---------|--------|
| Agent | `code_implementer` |
| First batch | 30 iterations |
| Until | `08:00` in `HTTPD_PLAN_TZ` (default `Europe/Berlin`; server clock may differ) |
| TZ | `export HTTPD_PLAN_TZ=Europe/Berlin` — **not** server local (e.g. US/Eastern is −6h vs CEST) |
| Gates | `HTTPD_GATES_SKIP_LIC_BUILD=1` when no clang |
| Pages | `refresh-live-sites.sh` after each success (`SKIP_BENCH=1` ok) |

## State and logs

- `data/httpd-plan-loop/state.json`
- `data/httpd-plan-loop/iter-*.log`, `overnight-*.log`, `until-deadline-*.log`

## Related

- `run-local-pages-benchmarks` — benchmarks + development overview deploy
- Plan: `docs/superpowers/plans/2026-05-16-li-httpd-plan.md`
- Baseline: `docs/ecosystem/httpd-m1-baseline.md`
