# Release notes: httpd plan autonomous loop

## Summary

Adds `scripts/httpd-plan-loop.py` and goal-directed loop (`code_implementer` + per-todo goal file) so Phase H httpd work can run in a loop without manual nudges every 30 minutes.

## Agent continuation

1. Read `docs/ecosystem/httpd-m1-baseline.md` and plan YAML todos in `docs/superpowers/plans/2026-05-16-li-httpd-plan.md`.
2. `export CURSOR_API_KEY=...` and `LI_CURSOR_AGENTS_ROOT=../li-cursor-agents`; `cd lic && ./scripts/httpd-plan-loop.py --once`.
3. Review PR from agent; run full CI; merge when green.
4. **Blocked:** Li routing binaries exit non-zero until CallProc string-to-extern ABI is fixed — gates use `HTTPD_SKIP_LI_ROUTING_BIN=1` until then.

## Changed

| Path | Change |
|------|--------|
| `scripts/httpd-plan-loop.py` | Parse plan todos, run gates, invoke `code_implementer` with todo goal via li-cursor-agents |
| `scripts/httpd-plan-gates.sh` | Build + `run_httpd_config.sh` subset |
| `docs/ecosystem/httpd-m1-baseline.md` | Merged/stale PR record |
| `.cursor/automations/httpd-plan-loop.md` | Automation doc |
| `packages/li-http/src/lib.li` | `li_rt_str_prefix_is_get`, single-call `parse_request` (E0311) |
| `runtime/li_rt.c`, `li_rt.h` | C helpers for GET probe + parse tag |
| `li-tests/run_httpd_config.sh` | `HTTPD_SKIP_LI_ROUTING_BIN` for compile-only gate |
| `li-cursor-agents` | `run-agent --goal-file` + `scripts/goal-directed-loop.sh` (`LI_REPO_WORKFLOW_REPO=lic`) |

## Not changed

- Full M1 plan completion (loop drives todos incrementally).
- `w0-lean-gate` / async reactor (deferred by pick order).
- Benchmark matrix refresh (agent must run per httpd rule after runtime edits).

## Breaking

N/A — dev tooling and test gate flags only.

## Security

N/A — no new trusted surface beyond existing `li_rt_str_*` helpers (audited in `trusted-c-audit.toml`).

## Performance

N/A — loop is offline orchestration.

## Downstream

- **benchmarks:** `scripts/httpd-plan-loop.sh` wrapper.
- **li-cursor-agents:** rebuild `npm run build` after pulling registry change.
