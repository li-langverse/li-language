# HTTPd M1 routing tests — table cases + overlap config_reject

## Summary

Completes plan todo **m1-routing-tests**: table-driven `match_route` cases under `li-tests/routing/`, overlap rejection in `li-tests/config_reject/`, `run_routing.sh` harness, and green Li `match_routes.li` / `match_routes_toml.li` oracles in `httpd-plan-gates.sh`.

## Agent continuation

1. Read `li-tests/routing/manifest.toml`, `scripts/httpd_match.py`, `scripts/httpd_config.py` (`[[routes]]` loader).
2. Run `./scripts/httpd-plan-gates.sh` and `./li-tests/run_routing.sh`.
3. **Next:** ~~`m1-toml-desugar`~~ (done); then **m1-core**.
4. **Blocked:** full `li-net-httpd` Li build without `import std.runtime.seam` (E0202 proxy externs).

## Changed

| Path | Change |
|------|--------|
| `li-tests/routing/cases/{method_reject,host_suffix}.toml` | Table cases vs `routing.toml` |
| `li-tests/routing/manifest.toml` | Case manifest |
| `li-tests/config_reject/routing_overlap.toml` | Same-priority overlap → reject |
| `li-tests/run_routing.sh` | Routing harness (Python + Li) |
| `scripts/httpd_config.py` | `[[routes]]` canonical parse for overlap fixtures |
| `scripts/httpd-plan-gates.sh` | Run `match_routes` binary + `run_routing.sh` |
| `li-tests/run_httpd_config.sh` | Delegate routing to `run_routing.sh` |

## Not changed

- `std/http/router.li` proved router (still C/Python oracles).
- Host-based route matching (cases document M1 ignores `host`).
- `serve_routed_once` TCP bind (skipped by default: `HTTPD_SKIP_SERVE_ROUTED_ONCE=1`).

## Breaking

N/A

## Security

Overlap reject prevents ambiguous same-priority routes at config load.

## Performance

N/A

## Downstream

CI `run_httpd_config.sh` includes full routing suite; plan loop advances to bearer-auth todo.
