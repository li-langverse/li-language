# HTTPd M1 — production serve (m1-serve-production)

## Summary

Completes plan todo **m1-serve-production**: long-lived `build/li-httpd` with **multi-worker** fork pool (`SO_REUSEPORT`), **HTTP/1.1 keep-alive**, **static** (argv epoll) and **reverse-proxy** (`.conf` epoll loop) — beyond `serve_once` / routed oracle stubs. Mixed static+proxy on one `.conf` uses `httpd_drain_slot_i` in the Li proxy loop for static routes.

## Agent continuation

1. **Gate:** `./scripts/test-serve-production.sh` after `./scripts/build-li-httpd.sh`.
2. **Full plan gates:** `./scripts/httpd-plan-gates.sh`.
3. **Workers:** `server.workers = 2` in TOML (flatten → `workers=2`) or `LI_HTTPD_WORKERS=auto|N` for argv mode.

## Changed

| Area | Paths |
|------|--------|
| Runtime | `runtime/li_rt_net.c` — `httpd_fork_workers_i`, `workers=` config, `SO_REUSEPORT` listen |
| Package | `packages/li-net-httpd/src/lib.li` — `httpd_begin_worker_pool` before production loops |
| Config | `scripts/flatten-httpd-config.py`, `examples/serve_production.toml` |
| Gates | `scripts/test-serve-production.sh`, `scripts/httpd-plan-gates.sh` |
| Plan | `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m1-serve-production: completed` |

## Not changed

- Tier-5 bench matrix / live Pages (no new CSV).

## Follow-up

- **m1-upstream-keepalive** completed in `docs/release-notes/2026-05-23-httpd-m1-upstream-keepalive.md`.

## Breaking

N/A.

## Security

Production gate uses loopback-only listeners; same path traversal / auth limits as other M1 runtime smokes.
