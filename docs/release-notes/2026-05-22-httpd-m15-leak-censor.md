# Release notes: httpd M1.5 leak_censor (m15-leak-censor)

**Branch:** `cursor/httpd-plan-continue`

## Summary

Optional upstream egress censorship: `setup-censor` generates `leak_censor.generated.toml`
from SQL migrations, validate-config merges deny_paths, runtime proxy relay redacts
built-in patterns (`openai_sk`, `jwt_bearer`, `pem_private`) when `[leak_censor] enabled = true`.

## Agent continuation

1. **Read** `scripts/schema_catalog.py`, `scripts/httpd_leak_censor.py`, `runtime/li_rt_net.c` (`leak_censor_prepare`).
2. **Run** `./scripts/check-httpd-leak-censor.sh` and `./scripts/httpd-plan-gates.sh`.
3. **Next** `m15-tls-auto` plan todo; wire `exploit_http.py` drivers under `benchmarks/tier5_http/harness/` when harness lands.
4. **Blocked** Full JSONPath walker in Li — deny_paths are config/merge only until `li-http` JSON scrub ships.

## Changed

- `scripts/setup-censor-httpd.py`, `scripts/schema_catalog.py`, `scripts/httpd_leak_censor.py`
- `scripts/check-httpd-leak-censor.sh`, `scripts/check-schema-catalog.sh`
- `runtime/li_rt_httpd.c`, `runtime/li_rt_net.c` — parse + proxy scrub + selftest
- `li-tests/schema_catalog/`, `li-tests/config_desugar/good/leak_censor_m15.toml`
- `li-tests/httpd/m15_leak_censor_oracle.li`
- `scripts/test-m15-leak-censor-runtime.sh`, `packages/li-net-httpd/examples/leak_censor_proxy.toml`
- `benchmarks/tier5_http/exploits/leak_*.toml` (Tier G manifest rows)
- `packages/li-http/src/leak_*.li`, `packages/li-schema/`

## Not changed

- TLS ACME (`m15-tls-auto`), live Pages bench refresh (`SKIP_BENCH=1`).

## Breaking / Security / Performance / Downstream

| Area | Note |
|------|------|
| Breaking | N/A — opt-in `leak_censor.enabled` |
| Security | Redacts known secret patterns on proxy egress; not a guarantee for unknown formats |
| Performance | Bounded scrub buffer 64KiB per relay chunk |
| Downstream | Tier G TOML + live proxy smoke (`test-m15-leak-censor-runtime.sh`); exploit drivers use oracle |

## Test plan

```bash
export PYTHONPATH=scripts
chmod +x scripts/check-httpd-leak-censor.sh scripts/check-schema-catalog.sh
./scripts/check-httpd-leak-censor.sh
./scripts/build.sh && ./scripts/build-li-httpd.sh
./scripts/test-m15-leak-censor-runtime.sh
./scripts/httpd-plan-gates.sh
```

Live sites: `SKIP_BENCH=1` (no new CSV).
