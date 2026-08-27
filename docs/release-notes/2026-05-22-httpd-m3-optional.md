# Release notes: httpd M3 optional (m3-optional)

**Branch:** `cursor/httpd-plan-continue`

## Summary

M3 optional agent-gateway hooks: RFC for L4 TCP stream proxy and `x-token-budget` ingress cap, config validate/flatten, runtime oracles, and 429 reject when declared budget exceeds schema cap.

## Agent deliverable

- [x] Branch pushed and PR opened (not draft)
- [x] CI triggered on PR
- [x] Tests added / updated — `li-tests/httpd/m3_optional_oracle.li`, `li-tests/config_desugar/good|reject/m3_*.toml`, `scripts/check-httpd-m3-config.sh`, `scripts/test-m3-token-budget-runtime.sh`
- [ ] Bench evidence — deferred (`stream_tcp` tier5 needs on-wire L4 relay)
- [x] Release notes — this file

## Changed

- `docs/superpowers/specs/2026-05-22-li-httpd-m3-l4-token-budget.md` — L4 + token-budget RFC
- `scripts/httpd_m3.py`, `scripts/check-httpd-m3-config.sh`, `scripts/httpd-plan-gates.sh`
- `li-tests/config_desugar/good/m3_optional.toml`, `reject/m3_*.toml` (incl. SSRF reject for loopback upstream)
- `li-tests/httpd/m3_optional_oracle.li`
- `runtime/li_rt_httpd.c`, `runtime/li_rt.h`, `runtime/li_rt_net.c` — M3 parse + token-budget 429 hook; TLS HTTP/1.1 recv/send via SSL (fixes m2-tls-h2-runtime smoke)
- `packages/li-net-httpd/examples/m3_token_budget.toml`, `scripts/test-m3-token-budget-runtime.sh` — live 429 smoke
- `scripts/httpd_config.py`, `scripts/flatten-httpd-config.py`
- `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m3-optional` → `completed`

## Not changed

- L4 byte relay (`splice` loop) on wire — M3.1 after async reactor
- Live GitHub Pages bench refresh (`SKIP_BENCH=1`)

## Test plan

```bash
chmod +x scripts/check-httpd-m3-config.sh scripts/test-m3-token-budget-runtime.sh
./scripts/check-httpd-m3-config.sh
./scripts/build-li-httpd.sh && ./scripts/test-m3-token-budget-runtime.sh
./scripts/httpd-plan-gates.sh
```
