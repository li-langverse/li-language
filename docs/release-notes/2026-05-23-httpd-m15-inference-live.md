# HTTPd M1.5 — inference live (/v1 runtime)

## Summary

Completes plan todo **m15-inference-live**: live `build/li-httpd` serves OpenAI-compatible `/v1` proxy routes with global rate limits (429), required `traceparent` on inference routes (400 without), upstream cancel when the client disconnects during SSE relay, and gate smoke `test-m15-inference-live.sh`.

## Agent continuation

1. **Run** `./scripts/test-m15-inference-live.sh` after `./scripts/build-li-httpd.sh`.
2. **Full plan gates:** `./scripts/httpd-plan-gates.sh`.
3. Example config: `packages/li-net-httpd/examples/inference_live.toml`.

## Changed

| Area | Paths |
|------|--------|
| Runtime | `runtime/li_rt_net.c` — Li-proxy ingress traceparent gate + inject export |
| Li proxy | `packages/li-net-httpd/src/lib.li` — OTel inject, cancel upstream on client hangup |
| Gates | `scripts/test-m15-inference-live.sh`, `scripts/httpd-plan-gates.sh` |
| Example | `packages/li-net-httpd/examples/inference_live.toml` |
| Plan | `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m15-inference-live: completed` |

## Not changed

- Tier-5 bench matrix / live Pages (no new CSV).

## Breaking

N/A.

## Security

Smoke uses loopback-only listeners; same SSRF/upstream allowlist as other M1 proxy gates.
