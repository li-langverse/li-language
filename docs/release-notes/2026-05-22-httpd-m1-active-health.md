# HTTPd M1 — active upstream health probes

## Summary

Completes plan todo **m1-active-health**: periodic `GET` probes on upstream peers (`[health.active]` in TOML), mark peers down on probe failure, and skip them in round-robin / least-conn rotation.

## Agent continuation

1. **Run** `./scripts/httpd-plan-gates.sh` (includes `test-active-upstream-health.sh` when `HTTPD_RUN_ACTIVE_HEALTH_TEST=1`, default on full build).
2. **Run** `./scripts/test-active-upstream-health.sh` after `./scripts/build-li-httpd.sh`.
3. Example config: `packages/li-net-httpd/examples/active_health.toml`.

## Changed

| Area | Paths |
|------|--------|
| Runtime | `runtime/li_rt_net.c` — `httpd_tick_active_health_probes_i`, probe in epoll + Li proxy loop |
| Seam | `std/runtime/seam.li`, `runtime/li_rt.h` |
| Proxy loop | `packages/li-net-httpd/src/lib.li` |
| Config | `scripts/flatten-httpd-config.py`, `scripts/validate-httpd-config.py` |
| Gates | `scripts/httpd-plan-gates.sh`, `scripts/test-active-upstream-health.sh` |
| Plan | `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m1-active-health: completed` |

## Not changed

- Tier-5 bench matrix ingest; live Pages refresh (no new CSV this slice).
- Passive health (`max_fails` on connect errors) behavior unchanged.

## Breaking

N/A — `[health.active]` is optional.

## Security

Probe path must be relative (`/…` only); validate rejects `://`, `..`, and `%`. Interval bounded to `[1, 300]` seconds.

## Performance

Probes run at most once per `health.active.interval` per upstream pool; blocking connect with 2s socket timeouts.

## Downstream

- **benchmarks** — re-run tier-5 `lb_peer_down` when refreshing httpd bench matrix.
