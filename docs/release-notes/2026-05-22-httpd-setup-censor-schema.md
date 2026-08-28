# Release notes: httpd setup-censor-schema

**Branch:** `cursor/httpd-plan-continue`

## Summary

`setup-censor-httpd.py` accepts `--migrations-applied migrations_applied.toml` so
deny_paths are generated only from SQL files already applied in production.
Users may disable censorship via `[leak_censor] enabled = false`; production
profile warns unless `ack_disable_censor = true`.

## Changed

- `scripts/schema_catalog.py` — `load_applied_manifest`, filtered `parse_migrations_dir`
- `scripts/setup-censor-httpd.py` — `--migrations-applied`
- `scripts/httpd_config.py` — print leak_censor validation warnings
- `scripts/check-schema-catalog.sh`, `scripts/check-httpd-leak-censor.sh`
- `li-tests/schema_catalog/migrations_applied.toml`, `004_billing_secret.sql`
- `li-tests/config_desugar/good/leak_censor_disabled.toml`

## Test plan

```bash
export PYTHONPATH=scripts
./scripts/check-schema-catalog.sh
./scripts/check-httpd-leak-censor.sh
HTTPD_GATES_SKIP_LIC_BUILD=1 ./scripts/httpd-plan-gates.sh
```

Live sites: not refreshed (`SKIP_BENCH=1`).
