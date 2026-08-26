# HTTPd M1 — Bearer auth gate green on build/li-httpd

## Summary

Completes plan todo **m1-bearer-auth**: Darwin/non-Linux link stub for `epoll_ctl_add_listen_i`, `scripts/build-li-httpd.sh`, and `httpd-plan-gates.sh` runs `test-auth-bearer.sh` against a real `build/li-httpd` binary.

## Agent continuation

1. **Run** `./scripts/httpd-plan-gates.sh` (includes bearer test when `HTTPD_RUN_BEARER_TEST=1`, default on full build — see **m0-ship-gate-full**).
2. **Run** `./scripts/test-auth-bearer.sh` after `./scripts/build-li-httpd.sh`.
3. **CI** builds `li-httpd` when present; bearer test is optional until Linux CI builds the binary every run.

## Changed

| Area | Paths |
|------|--------|
| Runtime stub | `runtime/li_rt_net.c` — `epoll_ctl_add_listen_i` on non-Linux |
| Build | `scripts/build-li-httpd.sh` |
| Gates | `scripts/httpd-plan-gates.sh` — build + `test-auth-bearer.sh` |
| Plan | `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m1-bearer-auth: completed` |

## Not changed

- Li async reactor; M1.5 TLS; tier-5 bench matrix ingest.
- Full strict-VC `lib.li` build without `--allow-open-vc`.

## Breaking

N/A

## Security

Bearer 401 on missing/invalid `Authorization: Bearer` when `[auth] require_bearer = true` in flattened runtime config (dev keys in TOML only).

## Performance

N/A

## Downstream

- **benchmarks** — no matrix change until tier-5 httpd bench is re-run with `LI_HTTPD_BIN`.
