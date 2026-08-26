# Release notes: httpd M1.5 TLS auto (m15-tls-auto)

**Branch:** `cursor/httpd-plan-continue`

## Summary

Automatic TLS provisioning for li-httpd: `manual`, `self_signed` (loopback or `dev=true`), and
`lets_encrypt` (staging/dry-run obtain + `acme-renewal.json` renew schedule). `setup-tls` writes
dev certs via OpenSSL; production ACME HTTP-01 remains gated until live challenge path ships.

## Changed

- `scripts/httpd_tls.py`, `scripts/setup-tls-httpd.py` — validate + provision + renew
- `scripts/check-httpd-tls-auto.sh`, `li-tests/tls_setup/run_setup_tls.sh`
- `runtime/li_rt_httpd.c`, `runtime/li_rt.h` — `[server.tls]` parse + oracle getters
- `li-tests/config_desugar/good/tls_*.toml`, `reject/tls_*.toml`
- `li-tests/httpd/m15_tls_oracle.li`
- `scripts/httpd_config.py`, `scripts/flatten-httpd-config.py` — TLS flatten keys
- Loopback listen on M1.5 agent/leak_censor test configs (public bind requires TLS)

## Not changed

- Full production ACME HTTP-01 against Let's Encrypt (use `environment = staging` or `--dry-run`)
- TLS 1.3 terminate on wire (M2 `m2-tls-h2`); live Pages bench refresh (`SKIP_BENCH=1`)

## Test plan

```bash
export PYTHONPATH=scripts
chmod +x scripts/check-httpd-tls-auto.sh li-tests/tls_setup/run_setup_tls.sh
./scripts/check-httpd-tls-auto.sh
HTTPD_GATES_SKIP_LIC_BUILD=1 ./scripts/httpd-plan-gates.sh
./scripts/build.sh && ./scripts/httpd-plan-gates.sh
```
