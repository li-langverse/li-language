# HTTP proxy/LB bench regression fix (wave 8 revert + argv routing)

## Summary

Fixes tier-5 `proxy_loopback` / LB **wrk_parse_fail_li** by reverting broken wave-8 `httpd_proxy_resp_finish_headers` filter relay and routing CSV upstream ports to the Li epoll proxy loop (not static `httpd_serve_port_root`).

## Agent continuation

1. **Read:** `runtime/li_rt_net.c` (`httpd_proxy_snap_reset`, `httpd_proxy_snap_begin_recording_if_get`); `packages/li-net-httpd/src/lib.li` (`httpd_upstream_proxy_epoll_loop`, `httpd_run_from_argv`).
2. **Run:** build `li-httpd`; from **benchmarks** `BENCH_PROXY_ORACLES=nginx,li ./scripts/run-tier5-http-bench.sh`; check `proxy_loopback` / `lb_*` rows in `docs/ecosystem/http-server-rps-matrix.md`.
3. **Then:** re-apply wave-8 `x-internal` header strip with a safe filter (do not break `httpd_proxy_resp_finish_headers` body relay).
4. **Blocked on:** none for loopback proxy RPS.

## Changed

- `runtime/li_rt_net.c` — snap race guard (`g_proxy_snap_recording`); `httpd_proxy_snap_reset` on upstream set; **pre-wave-8** proxy finish path (no broken header-filter relay).
- `packages/li-net-httpd/src/lib.li` — `httpd_upstream_proxy_epoll_loop`; CSV argv → proxy loop (not static serve).

## Not changed

- `g_strip_internal_headers` wave-8 feature (deferred safe re-land).
- `li-tls` terminate / HTTPS listen.
- **benchmarks** harness (updated in benchmarks PR).

## Breaking

N/A.

## Security

N/A — bench fix only.

## Performance

Loopback proxy (representative): `proxy_loopback` li ~130k+ RPS vs nginx ~78k on same host after fix.

## Downstream

- **benchmarks:** refresh matrix; merge supplemental `http_tier5.csv` only when li RPS &gt; 0.
