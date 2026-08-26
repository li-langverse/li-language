# Release notes: httpd M1.5 agent gateway (m15-agent)

**Branch:** `cursor/httpd-plan-continue`

## Summary

Agent-facing M1.5 slice: validate stream caps and idle/max duration in TOML, route
`x-model` → upstream pool via `[[route.match]]`, require `traceparent` on `/v1`
proxy routes, inject OTel traceparent when missing on the internal edge, detect
SSE (`text/event-stream`), enforce stream idle/max timeouts in the proxy relay,
and cancel upstream when the client disconnects (existing `httpd_proxy_clear`).

## Changes

- `scripts/httpd_m15.py` — M1.5 config oracle (stream limits, model routing, OTel require)
- `scripts/httpd_config.py` — `require=traceparent` route extra; M1.5 validation hook
- `scripts/flatten-httpd-config.py` — `stream_*`, `model_match`, `route_require` keys
- `scripts/check-httpd-m15-config.sh` — gate script
- `runtime/li_rt_httpd.c` — parse M1.5 limits; OTel/SSE oracle exports
- `runtime/li_rt_net.c` — model LB pick, traceparent inject/400, SSE stall 504, stream caps
- `li-tests/config_desugar/good/agent_m15.toml` + golden; reject fixtures
- `li-tests/httpd/m15_agent_oracle.li` — compile + runtime gate (`load_routes_from_m15_agent_fixture`, `LI_REPO_ROOT`)
- `runtime/li_rt_httpd.c` — `li_rt_httpd_load_m15_*_fixture` (avoid Li→C config path ABI)

## Test plan

```bash
export PYTHONPATH=scripts
./scripts/check-httpd-m15-config.sh
HTTPD_GATES_SKIP_LIC_BUILD=1 ./scripts/httpd-plan-gates.sh
./scripts/build.sh
./scripts/httpd-plan-gates.sh
```

Live sites: no tier-5 bench CSV change (`SKIP_BENCH=1`).
