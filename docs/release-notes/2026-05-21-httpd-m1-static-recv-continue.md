# HTTPd M1: static serve without index cache + config proxy loop

## Summary

Epoll static serving works when `index.html` is missing or too large for the cache; TOML config with upstream peers starts the Li proxy epoll loop instead of the static blocking path; `StringView` extern returns map to i8* for E0360.

## Agent continuation

1. **Read** `runtime/li_rt_net.c` (`httpd_prepare_root_i`, `httpd_try_drain_once`, `httpd_serve_conn_epoll`), `packages/li-net-httpd/src/lib.li` (`httpd_run_from_runtime_config`).
2. **Run** `LI_REPO_ROOT=$PWD ./li-tests/run_httpd_config.sh` and tier-5 `static_large` with `LI_HTTPD_BIN=build/li-httpd`.
3. **Then** land M1 exploits (`m1-wave4`) and `li-log`; rebase stacked PRs #87 / #84 if still open.
4. **Blocked on** full Lean discharge on server modules (P0-lean); M1.5 SSE/TLS not started.

## Changed

| Area | Paths |
|------|--------|
| Static recv | `runtime/li_rt_net.c` — serve on `g_doc_root_len`; optional index cache |
| Config proxy | `packages/li-net-httpd/src/lib.li` — `httpd_upstream_proxy_epoll_loop` from `.conf` |
| Proxy detect | `httpd_proxy_configured_i` — peers or `g_proxy_port` |
| ABI | `compiler/mir/lower.cpp` — `StringView` extern return width |
| Tracker | `docs/superpowers/plans/2026-05-14-li-master-plan.md` Phase H rows |

## Not changed

- M1.5 SSE, TLS ACME, leak_censor.
- `li-log` package / rotation.
- Exploit tiers A+B ship gate scripts.
- Li async reactor (still C epoll).

## Breaking

N/A

## Security

N/A — same path traversal checks in `httpd_try_drain_once`.

## Performance

N/A — enables `static_large` when index cache is absent; no bench threshold change.

## Downstream

- **benchmarks** — refresh matrix after merge; `static_large` should report RPS not `wrk_parse_fail_li`.
