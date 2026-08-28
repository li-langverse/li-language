# Release notes: M1 TOML route loader (Li API)

## Summary

Phase **H M1** loads `[routes]` from `li-tests/httpd/fixtures/routing.toml` at runtime via `load_routes_from_routing_fixture()` and `match_route()` in `packages/li-http`, backed by trusted `runtime/li_rt_httpd.c` linked on every `lic build`.

## Agent continuation

1. Read `runtime/li_rt_httpd.c`, `packages/li-http/src/lib.li`, `li-tests/routing/match_routes_toml.li`.
2. Run `LI_REPO_ROOT=$PWD LI_ALLOW_OPEN_VC=1 ./li-tests/run_httpd_config.sh` and `./li-tests/run_all.sh routing`.
3. Then: pure-Li TOML parser (drop C table parse), `explain-config` CLI, overlap reject surfaced as `lic` diagnostic.
4. Blocked on: `std/io` real `file_read_all`; argv-as-`str` for config path CLI.

## Changed

| Path | Change |
|------|--------|
| `runtime/li_rt_httpd.c` | Parse `[routes]`, validate overlap, priority match |
| `runtime/li_rt.h`, `runtime/CMakeLists.txt` | New symbols |
| `compiler/codegen/compile.cpp` | Link `li_rt_httpd.c` on user binaries |
| `packages/li-http/src/lib.li` | `load_routes_from_toml`, `match_route`, `route_action_kind` |
| `li-tests/routing/match_routes_toml.li` | Oracle vs `routing.toml` |
| `li-tests/run_httpd_config.sh` | Li TOML loader step |
| `li-tests/manifest.toml` | `routing/match_routes_toml.li` |
| Master plan + httpd plan + `httpd-prerequisites.md` | M1 TOML loader partial |

## Not changed

- Full TOML config (`[server]`, `[limits]`, upstreams) in Li.
- li-httpd TCP serve / epoll reactor.
- Python `httpd_config.py` remains CI oracle for desugar/reject.
- Proof discharge for route invariants (**P-http**).

## Breaking

N/A

## Security

Trusted C reads config path from `LI_REPO_ROOT`; path traversal rejected on route keys (`..`, `//`). Audit row in `security/trusted-c-audit.toml`.

## Performance

N/A — load once per process; linear scan ≤64 routes.

## Downstream

`li-net-httpd` can call `http.match_route` after loading user config path when file I/O ships.
