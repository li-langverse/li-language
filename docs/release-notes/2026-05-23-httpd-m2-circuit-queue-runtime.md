# Release notes: httpd M2 circuit + queue runtime (m2-circuit-queue-runtime)

**Branch:** `cursor/httpd-plan-continue`

## Summary

Live `build/li-httpd` enforces M2 cold-queue depth and upstream circuit breaker: saturated peers return **429** with configurable **Retry-After** before opening new proxy streams.

## Changed

- `runtime/li_rt_net.c` — access-log on queue 429; proxy errors mark upstream not reusable (circuit failures); 429 when proxy start fails with no healthy peers
- `packages/li-net-httpd/examples/circuit_queue.toml` — minimal M2 queue + circuit config for smokes
- `scripts/test-m2-circuit-queue-runtime.sh`, `scripts/httpd-plan-gates.sh` — runtime gate (opt-out `HTTPD_RUN_M2_CIRCUIT_QUEUE_TEST=0`)
- `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m2-circuit-queue-runtime` → `completed`

## Tests

```bash
./scripts/build-li-httpd.sh
./scripts/test-m2-circuit-queue-runtime.sh
./scripts/httpd-plan-gates.sh
```

## Breaking

N/A — behavior applies only when `[route.queue]` and/or `[circuit_breaker]` are present (M2 profile).
