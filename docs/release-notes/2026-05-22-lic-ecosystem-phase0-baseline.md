# Release notes: lic ecosystem phase 0 baseline

## Summary

Phase 0 for the scientific/engineering/gaming package loop: import algorithms-and-libraries plan, ecosystem baseline doc, and agent-kit skill for local CI when GHA quota is exhausted. No httpd/runtime changes.

## Agent continuation

1. Branch `cursor/lic-ecosystem-plan-loop-54aa`.
2. Run `./scripts/local-ci.sh` before opening PR (GHA quota).
3. Read `docs/ecosystem/lic-ecosystem-baseline.md` and `algorithms-and-libraries-plan.md` §12.
4. Webserver work stays on `cursor/httpd-plan-loop-54aa` (other agent).

## Changed

| Path | Change |
|------|--------|
| `docs/ecosystem/algorithms-and-libraries-plan.md` | §7 package gap register + §12 handoff |
| `docs/ecosystem/lic-ecosystem-baseline.md` | Phase 0 branch/CI/httpd separation |
| `.cursor/rules/li-local-actions-quota.mdc` | Link to `run-local-ci-gha-quota` skill |

## Not changed

- `runtime/li_rt_httpd.c`, httpd plan loop, M1 httpd todos
- `lic-ecosystem-plan-loop.py` (phase 1)

## Breaking

N/A — docs and agent guidance only.

## Security

N/A

## Performance

N/A

## Downstream

- **roadmap/agent-kit** 1.3.3 — sync `run-local-ci-gha-quota` skill
- **li-local-ci** — canonical skill copy in-repo
