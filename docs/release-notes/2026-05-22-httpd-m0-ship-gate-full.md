# HTTPd M0 — full ship gate (lic + li-httpd + bearer smoke)

## Summary

Completes plan todo **m0-ship-gate-full**: `httpd-plan-gates.sh` builds `lic`, compiles `build/li-httpd` from `packages/li-net-httpd/src/main.li`, and runs `test-auth-bearer.sh` against a live server (no `HTTPD_GATES_SKIP_LIC_BUILD`).

## Agent continuation

1. **Run** `./scripts/httpd-plan-gates.sh` (full gate; bearer on by default).
2. **Opt out** bearer only: `HTTPD_RUN_BEARER_TEST=0 ./scripts/httpd-plan-gates.sh`.
3. **Python-only** slice: `HTTPD_GATES_SKIP_LIC_BUILD=1 HTTPD_RUN_BEARER_TEST=0 ./scripts/httpd-plan-gates.sh`.

## Changed

| Area | Paths |
|------|--------|
| Build | `scripts/build-li-httpd.sh` — `main.li`, `llvm-env.sh`, `--no-lean-verify` |
| Gates | `scripts/httpd-plan-gates.sh` — `build-li-httpd` + default `HTTPD_RUN_BEARER_TEST=1` |
| Plan | `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m0-ship-gate-full: completed` |

## Not changed

- Strict Lean on full `lib.li` (open VC + `--no-lean-verify` for ship binary).
- Tier-5 bench matrix ingest / live Pages refresh.

## Breaking

N/A

## Security

Bearer 401/200 smoke on flattened runtime config (dev keys in example TOML).

## Performance

N/A

## Downstream

- **benchmarks** — refresh when tier-5 httpd runtime benches run with `LI_HTTPD_BIN=build/li-httpd`.
