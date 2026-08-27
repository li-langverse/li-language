# RFC: li-httpd M3 — L4 stream proxy + token-budget hooks

**Status:** draft — config + oracle shipped; on-wire L4 splice deferred.  
**Milestone:** M3 optional (`m3-optional`).  
**Plan:** [2026-05-16-li-httpd-plan.md](../plans/2026-05-16-li-httpd-plan.md).

## Goal

Two optional agent-gateway features that stay **out of the M1 LOC cap** until explicitly enabled:

1. **L4 stream** — byte-transparent TCP proxy for tool/gRPC-adjacent workloads (not HTTP parsing).
2. **Token budget hook** — cooperate with upstream apps via `x-token-budget` (or configured header) as an extra rate dimension before proxy/SSE.

Nginx remains the bench/CVE oracle; Li proves policy in config + bounded runtime checks.

## L4 stream (optional)

### Config (`[server.l4_stream]`)

| Key | Type | Rule |
| --- | ---- | ---- |
| `enabled` | bool | default `false` |
| `listen` | `host:port` | required when enabled; loopback or TLS-terminated public listener |
| `upstream_host` | hostname | required; no userinfo; no private/loopback unless `allow_private_upstream = true` |
| `upstream_port` | 1..65535 | required |
| `max_connections` | int | schema cap 10_000; default 256 |
| `allow_private_upstream` | bool | default `false`; set `true` for dev/oracle loopback upstream |

### Semantics (target)

- Separate accept socket from HTTP `server.listen`; no HTTP parser on this path.
- `connect(upstream)` + `splice`/`send` relay with byte caps per connection (reuse `limits.max_body` scale).
- Idle timeout inherits `limits.stream_idle_timeout` when set.
- **v1 shipped:** validate + flatten + oracle getters only; relay loop is **M3.1** after `w1-async-reactor`.

### LOC justification

| Component | Est. LOC | Notes |
| --------- | -------- | ----- |
| Config validate/flatten | ~120 `.py` | schema caps, SSRF guard on upstream |
| Runtime oracle | ~150 `.c` | parse TOML + flattened `runtime.conf` |
| L4 relay (deferred) | ~400 `.c` | epoll edge-triggered splice; counted against M3 cap |

## Token budget hook

### Config (`[limits.token_budget]`)

| Key | Type | Rule |
| --- | ---- | ---- |
| `enabled` | bool | default `false` |
| `header` | string | must match `x-[a-z0-9-]+`; default `x-token-budget` |
| `max_per_request` | int | 1..10_000_000 — declared budget ceiling per request |
| `reject_over_cap` | bool | default `true` — 429 when header integer exceeds cap |

### Request path (v1)

1. After header parse, before global/route rate bucket:
   - If disabled → no-op.
   - If header absent → pass (app may omit on non-inference routes).
   - If header present → parse base-10 integer; reject non-numeric.
   - If `value > max_per_request` → **429** `Token Budget Exceeded` + `Retry-After: 1`.
   - If `value == 0` → **429** (no budget remaining).
2. Audit log field `token_budget_declared` when present (M1.5 `li-log` sink; wire in M3.1).

### Rate dimension

Adds orthogonal dimension to `[route.rate_limit]` keys (`api_key`, `agent_id`):

- Dimension name: `token_budget` (enum in validator).
- v1 does **not** maintain cross-request debit ledger — apps cooperate by monotonically lowering the header; gateway enforces per-request cap only.
- Future: shared-memory debit per `api_key` hash when `debit_mode = "shared"` (RFC extension).

## Security

| Threat | Mitigation |
| ------ | ---------- |
| L4 open proxy | upstream host allowlist at validate; no dynamic peer from headers |
| Header injection | `header` must be `x-*` lowercase; not `Authorization` / `Host` |
| Budget bypass | `reject_over_cap`; missing header does not disable other limits |
| Integer overflow | parse into `int64`; compare before use |

## Exit gate (m3-optional todo)

- [x] This RFC
- [x] `scripts/httpd_m3.py` validate + flatten
- [x] `li-tests/config_desugar/good/m3_optional.toml` + `reject/m3_*.toml`
- [x] `li-tests/httpd/m3_optional_oracle.li` + `check-httpd-m3-config.sh`
- [x] Runtime oracle getters + ingress 429 hook (cap only)
- [x] `scripts/test-m3-token-budget-runtime.sh` on live `build/li-httpd`
- [ ] L4 byte relay on wire (M3.1)
- [ ] tier5 `stream_tcp` scenario enabled (needs `tcpkali` + relay)

## Learned from

| Source | Keep | Reject |
| ------ | ---- | ------ |
| LiteLLM budgets | per-key spend caps | 50-model admin UI |
| APISIX AI gateway | token rate limits | Lua plugins |
| nginx `stream {}` | separate L4 module | full `stream` DSL parity |
