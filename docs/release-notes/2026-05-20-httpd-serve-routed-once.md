# Release notes: serve_routed_once (M1 route-matched response)

## Summary

Adds **`httpd_serve_routed_once`**: after loading `routing.toml`, handles one stub accept with a built-in **`GET /health`** request, **`match_route`**, and a **static 200** (or 404/502 by action kind). Complements draft work on **#84** (epoll perf) and **#87** (M1 wave 1) without duplicating their serve loops.

## Agent continuation

1. Read `runtime/li_rt_httpd.c` (`li_rt_httpd_serve_routed_once`, `parse_http_request_line`).
2. Run `LI_REPO_ROOT=$PWD LI_ALLOW_OPEN_VC=1 ./li-tests/run_httpd_config.sh` and `./li-tests/run_all.sh httpd`.
3. Then: wire real `tcp_recv` + request buffer; integrate with **httpd-m1-impl** / **httpd-m1-perf** when those merge.
4. Blocked on: production reactor and proxy upstream (parallel PRs).

## Changed

| Path | Change |
|------|--------|
| `runtime/li_rt_httpd.c` | `serve_routed_once`, minimal request-line parse, status responses |
| `packages/li-net-httpd/src/lib.li` | `httpd_serve_routed_once` |
| `li-tests/httpd/serve_routed_once.li` | Oracle test |
| `li-tests/run_httpd_config.sh` | Routed serve step |

## Not changed

- `li_rt_net.c` stub fds (no real sockets).
- Epoll/kqueue (**cursor/httpd-m1-perf-54aa**).
- Generic static file server (**cursor/httpd-m1-impl-54aa**).
- `httpd_serve_once` (unchanged blind 200).

## Breaking

N/A

## Security

Stub in-process only; fixed request line for M1 oracle.

## Performance

N/A

## Downstream

Rebase **httpd-m1-impl** / **httpd-m1-perf** onto this: replace built-in request with recv buffer when their net stack lands.
