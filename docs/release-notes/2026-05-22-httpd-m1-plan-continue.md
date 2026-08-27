# HTTPd M1 plan continue — overlap, validate/flatten, Bearer auth

## Summary

Advances Phase H M1: route overlap rejection (Python + `httpd_config.py`), `validate-httpd-config` / `flatten-httpd-config` tooling, runtime Bearer 401 gate, and `lic validate-httpd-config` CLI alias.

## Agent continuation

1. **Read** `scripts/validate-httpd-config.py`, `scripts/flatten-httpd-config.py`, `runtime/li_rt_net.c` (`httpd_auth_request_ok_c`).
2. **Run** `./li-tests/run_httpd_config.sh` (overlap + desugar + reject); `./scripts/lic-validate-httpd-config.sh packages/li-net-httpd/examples/auth_bearer.toml`.
3. **Then** rebuild `packages/li-net-httpd/src/lib.li` → `build/li-httpd` (fix `import std.runtime.seam` visibility for proxy symbols) and run `./scripts/test-auth-bearer.sh`.
4. **Blocked on** `match_routes.li` E0311 borrow (pre-existing); full Li httpd lib build; M1.5 TLS/SSE.

## Changed

| Area | Paths |
|------|--------|
| Overlap | `scripts/httpd_config.py`, `scripts/check-httpd-overlap-reject.py` |
| Validate/flatten | `scripts/validate-httpd-config.py`, `scripts/flatten-httpd-config.py`, `scripts/lic-validate-httpd-config.sh` |
| Auth | `runtime/li_rt_net.c`, `packages/li-net-httpd/examples/auth_bearer.toml`, `scripts/test-auth-bearer.sh` |
| CLI | `compiler/lic/main.cpp` — `lic validate-httpd-config` |
| CI | `scripts/ci.sh`, `li-tests/run_httpd_config.sh` (shell `rc=$?` fix) |
| Explain link | `scripts/check-httpd-explain-config.sh` — `li_rt_log.c` |

## Not changed

- Li async reactor (`w1-async-reactor`).
- Full `packages/li-net-httpd/src/lib.li` build (proxy extern visibility).
- M1.5 TLS ACME, leak_censor, audit JSONL.
- benchmarks matrix ingest.

## Breaking

N/A

## Security

Bearer API key gate on epoll path when `auth_required=1` in flattened runtime config; keys from TOML `[auth]` (dev profile only — hash at edge in M1.5).

## Performance

N/A

## Downstream

- **benchmarks** — no catalog change; tier5 exploits unchanged.
