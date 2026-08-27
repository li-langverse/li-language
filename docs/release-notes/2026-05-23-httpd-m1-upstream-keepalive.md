# HTTPd M1 — upstream HTTP keepalive pool

## Summary

Completes plan todo **m1-upstream-keepalive**: reuse TCP connections to inference backends with stale-fd detection, response-driven pool release, explicit `Connection: keep-alive` on proxied requests, and one-shot reconnect on `EPIPE`/`ECONNRESET` (avoids stale-pool 502s).

## Agent continuation

1. **Run** `./scripts/test-upstream-keepalive.sh` after `./scripts/build-li-httpd.sh`.
2. **Full plan gates:** `./scripts/httpd-plan-gates.sh`.
3. Example config: `packages/li-net-httpd/examples/upstream_keepalive.toml`.

## Changed

| Area | Paths |
|------|--------|
| Runtime | `runtime/li_rt_net.c` — pool acquire/release, upstream keep-alive header, reconnect |
| Gates | `scripts/test-upstream-keepalive.sh`, `scripts/httpd-plan-gates.sh` |
| Example | `packages/li-net-httpd/examples/upstream_keepalive.toml` |
| Plan | `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m1-upstream-keepalive: completed` |

## Not changed

- Tier-5 bench matrix / live Pages (no new CSV).

## Breaking

N/A.

## Security

Smoke uses loopback-only listeners; same SSRF/upstream allowlist as other M1 proxy gates.
