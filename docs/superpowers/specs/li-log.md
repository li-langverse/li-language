# li-log (M1)

Logging sinks for li-httpd: access lines, redaction, file rotation seam.

## Runtime

- `runtime/li_rt_log.c` — `li_rt_log_access_line`, `li_rt_log_redact`, size-based rotate at 100MiB.
- Default directory: `./logs/access.log` (mode 0640 intent; dir 0750).

## Package

- `packages/li-log` — `redact_log`, `log_access`, `log_set_dir`, `log_reopen`.
- Import: `import log`.

## M1 scope

| Shipped | Deferred |
|---------|----------|
| Bearer / Cookie / x-api-key / sk- redaction | Async bounded queue |
| Access file sink + stderr fallback | Audit JSONL sink |
| Rotate to `access.log.1` at max size | gzip compress backups |

## Tests

- `li-tests/log/redact_bearer.li`
- `li-tests/composable/import_log_lib.li`
