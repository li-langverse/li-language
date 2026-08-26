# li-log M1 package + httpd access sink

## Summary

Adds `packages/li-log` with Bearer/sk- redaction tests, `runtime/li_rt_log.c` access file sink (rotate at 100MiB), and wires epoll httpd access lines through `li_rt_log_access_line`.

## Agent continuation

1. **Read** `packages/li-log/src/lib.li`, `runtime/li_rt_log.c`, `docs/superpowers/specs/li-log.md`.
2. **Run** `LI_ALLOW_OPEN_VC=1 ./build/compiler/lic/lic build li-tests/log/redact_bearer.li -o /tmp/t && /tmp/t` (expect exit 0); `./li-tests/run_all.sh log` when manifest suite filter exists.
3. **Then** merge with `cursor/httpd-m1-continue-54aa` or rebase onto `main`; add TOML `[log]` desugar + audit JSONL (M1.5).
4. **Blocked on** full `raises Log` effect typing and gzip rotation — deferred per httpd plan.

## Changed

| Area | Paths |
|------|--------|
| Package | `packages/li-log/` (scaffold `li-new-package`, `import_name = "log"`) |
| Runtime | `runtime/li_rt_log.c`, `li_rt.h`, `runtime/CMakeLists.txt`, `compiler/codegen/compile.cpp` |
| httpd | `runtime/li_rt_net.c` — `httpd_access_log` → `li_rt_log_access_line` |
| Tests | `li-tests/log/redact_bearer.li`, `li-tests/composable/import_log_lib.li`, `li-tests/manifest.toml` |
| Docs | `docs/superpowers/specs/li-log.md`, workspace `packages/li.toml` |

## Not changed

- Audit JSONL sink, async log queue, `[log]` TOML desugar in validate-config.
- M1.5 SSE/TLS/leak_censor.
- Li async reactor (still C epoll).
- benchmarks matrix (no ingest required for this change).

## Breaking

N/A

## Security

Redacts `Authorization`/`Cookie`/`x-api-key` substrings and `sk-*` API key patterns before access log write; CWE-117 log injection mitigated for those patterns (see `security/webserver-bugs.toml` li_rule).

## Performance

N/A — synchronous `fputs`+`fflush` per request; bounded rotate rename only at 100MiB.

## Downstream

- **li-httpd** binary may create `./logs/access.log` on first request when using C epoll path.
