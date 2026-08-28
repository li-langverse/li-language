# Release notes: httpd M1 core — proxy rate-limit validation

**PR:** #173 (`cursor/httpd-plan-loop-54aa`)

## Summary

M1 agent-gateway configs with `proxy:` routes must declare global token-bucket limits
(`limits.rate_limit_rps`, optional `rate_limit_burst`) so public/agent paths cannot ship
without abuse caps. Validated in Python (`validate-httpd-config.py`) before runtime Li build.

## Changes

- `scripts/validate-httpd-config.py` — require `rate_limit_rps` when any `proxy:` route exists; cap range 1–100k
- `runtime/li_rt_httpd.c` — same policy in `lic httpd validate-config` (C `[limits]` + `[routes]` scan)
- `li-tests/config_desugar/reject/proxy_without_rate_limit.toml` — negative case
- `packages/li-net-httpd/examples/agent_gateway_limits.toml` — documented good example
- `li-tests/config_desugar/good/agent_gateway.toml` — adds rate limits + `max_header`
- `packages/li-http/src/lib.li` — `lb_mode_from_name` (round_robin / least_conn enum)
- `li-tests/httpd/lb_mode_smoke.li` — M1 LB parser surface smoke
- `scripts/httpd-plan-loop.py` — prefer M1 todos over w0/w1 blockers; goal-directed `code_implementer`
- `scripts/httpd-plan-gates.sh` — optional `HTTPD_GATES_SKIP_LIC_BUILD=1` for Python-only CI

## Test plan

```bash
./scripts/build.sh
./scripts/build-li-httpd.sh
./scripts/httpd-plan-gates.sh
./li-tests/run_httpd_config.sh
lic httpd validate-config li-tests/config_desugar/reject/proxy_without_rate_limit.toml && exit 1 || true
```
