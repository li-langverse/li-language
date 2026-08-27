# Release notes: httpd M2 runtime parity (m2-tls-h2 slice)

**Branch:** `cursor/httpd-plan-continue` · **PR:** #175

## Summary

Completes M2 runtime parity for WebSocket proxying, webhook egress SSRF guard, and fixes queue/circuit-breaker 429 behavior when the GET proxy snap cache was bypassing live upstream policy.

## Changed

- `runtime/li_rt_net.c` — disable proxy snap under M2 queue/circuit; WebSocket tunnel (`PROXY_RESP_BODY_TUNNEL`); `X-Li-Webhook-Url` allowlist; circuit failures on proxy errors
- `scripts/test-m2-websocket-runtime.sh`, `scripts/test-m2-webhook-egress-runtime.sh` — live smokes
- `packages/li-net-httpd/examples/websocket_proxy.toml`, `webhook_egress.toml`
- `scripts/httpd-plan-gates.sh` — wire M2 WS + webhook runtime gates
- `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m2-websocket-runtime`, `m2-webhook-egress-runtime` → completed

## Test plan

```bash
./scripts/build-li-httpd.sh
./scripts/test-m2-tls-h2-runtime.sh
./scripts/test-m2-circuit-queue-runtime.sh
./scripts/test-m2-websocket-runtime.sh
./scripts/test-m2-webhook-egress-runtime.sh
./scripts/httpd-plan-gates.sh
```
