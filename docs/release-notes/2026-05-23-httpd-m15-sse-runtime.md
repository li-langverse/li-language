# Release notes: httpd M1.5 SSE runtime (m15-sse-runtime)

**Branch:** `cursor/httpd-plan-continue`

## Summary

Live `build/li-httpd` now enforces `limits.stream_idle_timeout` during chunked SSE proxy
relay: inter-chunk stalls return **504 Gateway Timeout** and close the upstream connection.
Idle detection runs on a periodic tick (not only on epoll events), and chunk timestamps
update for chunked upstream bodies relayed to the client.

## Changes

- `runtime/li_rt_net.c` — SSE chunk timestamps on body relay; idle tick + bounded epoll in C and Li proxy loops; upstream cancel on stall
- `packages/li-net-httpd/src/lib.li` — `httpd_upstream_proxy_epoll_loop` calls SSE idle tick + timed epoll wait
- `std/runtime/seam.li` — exports for SSE idle tick and timed epoll wait
- `packages/li-net-httpd/examples/sse_idle.toml` — short idle timeout for gate smoke
- `scripts/test-m15-sse-runtime.sh` — live SSE relay + stall cancel gate
- `scripts/httpd-plan-gates.sh` — wire m15-sse-runtime smoke

## Test plan

```bash
./scripts/build-li-httpd.sh
./scripts/test-m15-sse-runtime.sh
./scripts/httpd-plan-gates.sh
```

Live sites: no tier-5 bench CSV change (`SKIP_BENCH=1`).
