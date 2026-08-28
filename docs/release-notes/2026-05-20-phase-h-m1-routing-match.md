# Release notes: Phase H M1 — routing match (li-http)

## Summary

First **M1 `.li` routing** piece: `match_route_fixture` in `packages/li-http`, trusted `li_rt_match_route_fixture` C oracle aligned with `li-tests/httpd/fixtures/routing.toml`, plus `li-tests/routing/match_routes.li` and CI hook in `run_httpd_config.sh`.

## Agent continuation

1. Read `packages/li-http/src/lib.li`, `runtime/li_rt.c` (`li_rt_match_route_fixture`), `scripts/check-httpd-route-fixture.sh`.
2. Run `./li-tests/run_httpd_config.sh` and `LI_ALLOW_OPEN_VC=1 LIC=./build/compiler/lic/lic build li-tests/routing/match_routes.li`.
3. Next: load routes from desugared TOML in Li (not fixture-hardcoded); extend `match_route` beyond `match_route_fixture` oracle.

## Changed

| Path | Change |
|------|--------|
| `runtime/li_rt.{c,h}` | `li_rt_str_eq`, `li_rt_path_*`, `li_rt_match_route_fixture` |
| `packages/li-http/src/lib.li` | `match_route_fixture`, action kind helpers |
| `li-tests/routing/match_routes.li` | Li tests (compile gate) |
| `scripts/check-httpd-route-fixture.sh` | C oracle vs `api_prefix.toml` |
| `li-tests/run_httpd_config.sh` | Li build + C oracle after Python cases |

## Not changed

- Full TOML route table loader in Li.
- li-httpd binary serve loop.
- str move ABI beyond `i8*` call args (full `str` stdlib semantics).

## Breaking

N/A

## Security

Trusted C path compare only (documented in `trusted-c-audit` follow-up).

## Performance

N/A

## Downstream

`li-net-httpd` can call `http.match_route_fixture` once config desugar lands.
