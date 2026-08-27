# Release notes: httpd M2 TLS/H2 runtime (m2-tls-h2-runtime)

**Branch:** `cursor/httpd-plan-continue`

## Summary

Live `build/li-httpd` terminates **TLS 1.3** on the listener (OpenSSL 3 via `dlopen`), negotiates **ALPN** (`h2`, `http/1.1`), and serves a minimal **HTTP/2** `GET /health` path for agent-gateway smokes.

## Changed

- `runtime/li_rt_tls.c`, `runtime/li_rt_h2.c` — TLS accept + minimal H2 frames/HPACK
- `runtime/li_rt_net.c` — flattened `tls_*` / `m2_tls_*` keys; TLS read/write on client slots
- `packages/li-net-httpd/src/lib.li` — handshake + H2 dispatch on accept
- `packages/li-net-httpd/examples/tls_h2.toml`, `scripts/test-m2-tls-h2-runtime.sh`, `scripts/httpd-plan-gates.sh`
- `compiler/codegen/compile.cpp`, `scripts/build-li-httpd.sh` — link TLS/H2 runtime + `-ldl`
- `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` — `m2-tls-h2-runtime` → `completed`

## Tests

```bash
./scripts/build-li-httpd.sh
./scripts/test-m2-tls-h2-runtime.sh
./scripts/httpd-plan-gates.sh
```

## Breaking

N/A — TLS/H2 activate only when `[server.tls] terminate` and `[server.http2] enabled` are set (M2 profile).
