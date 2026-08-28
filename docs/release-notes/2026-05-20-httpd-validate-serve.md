# Release notes: validate-config CLI + serve-once stub

## Summary

Adds **`lic httpd validate-config`** with **E0501–E0504** diagnostics, **`route_key_valid`** on the http API, and **`httpd_serve_once`** M1 TCP stub (listen → accept → `200 OK`).

## Agent continuation

1. Read `compiler/lic/main.cpp` (`httpd_validate_config`), `runtime/li_rt_httpd.c` (errors + `li_rt_httpd_serve_once`).
2. Run `./li-tests/run_httpd_config.sh` and `./li-tests/run_all.sh httpd`.
3. Then: pure-Li TOML scan when `str` can be read without move; event-loop serve; config line/column in diagnostics.
4. Blocked on: repeated `str` use in one proc (**E0311** move) — `route_key_valid` uses trusted C until borrow/read-only `str` view lands.

## Changed

| Path | Change |
|------|--------|
| `compiler/lic/main.cpp` | `lic httpd validate-config`; links `li_rt` |
| `compiler/diagnostics/` | **E0501–E0504** httpd.config.* |
| `runtime/li_rt_httpd.c` | `last_error_*`, `route_key_valid`, `serve_once` |
| `runtime/li_rt.c` | `li_rt_str_len`, `li_rt_str_char_at` |
| `packages/li-http/src/lib.li` | `route_key_valid` |
| `packages/li-net-httpd/src/lib.li` | `httpd_serve_once` |
| `li-tests/httpd/route_key_valid.li`, `serve_once_smoke.li` | New |
| `li-tests/run_httpd_config.sh` | validate-config gate |

## Not changed

- Full TOML parse in Li source (still C loader for files).
- Epoll/kqueue reactor or streaming.
- Real upstream proxy on matched routes.

## Breaking

N/A

## Security

`serve_once` stub only; no network bind in CI (compile gate). Config path validated for traversal in route keys.

## Performance

N/A

## Downstream

`li-httpd` binary can call `httpd_serve_once` in smoke tests; production serve loop still M1 backlog.
