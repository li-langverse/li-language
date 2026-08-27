# tier5_http — li-httpd / nginx benchmark harness

All scenario config lives in **TOML** (`defaults.toml`, `suite.toml`, `scenarios/*/bench.toml`).
Python harnesses read TOML only — do not hardcode scenario names in `bench_http.py`.

## Quick start

```bash
# CI profile: verify only (stub static server if nginx missing)
TIER5_HTTP_STUB=1 ./benchmarks/harness/bench_http.py --profile ci

# Merge/validate without servers
./benchmarks/harness/bench_http.py --dry-run

# Nginx baseline timing (needs nginx + wrk)
./benchmarks/harness/bench_http.py --profile baseline

# Single scenario + override
./benchmarks/harness/bench_http.py static_small --set load.connections=500

# Render nginx.conf from merged TOML
./benchmarks/harness/bench_http.py static_small --render-nginx-only /tmp/nginx.conf
```

## Layout

| Path | Role |
|------|------|
| `defaults.toml` | Global workers, ports, load tool defaults |
| `suite.toml` | Profiles (`ci`, `baseline`, …) and include/exclude |
| `scenarios/<name>/bench.toml` | Self-contained scenario |
| `templates/nginx.conf.in` | Rendered per scenario for nginx parity |
| `../harness/bench_http.py` | Orchestrator |
| `../harness/verify_http.py` | `[verify]` gate |
| `params_schema.toml` | Key reference |

Exploit manifests: `suite_exploits.toml`, `exploits/*.toml`, `nginx_mitigations.toml`.

Nginx oracle (read-only): `third_party/nginx` submodule + `../harness/audit_nginx_src.py` — see `docs/security-nginx-src-audit.md`.

```bash
# CI / plan gates (stub — no nginx required)
TIER5_EXPLOIT_STUB=1 ./benchmarks/harness/exploit_http.py --profile pr

# Merge/validate only
./benchmarks/harness/exploit_http.py --dry-run

# Nginx baseline vs li-httpd (needs nginx + build/li-httpd)
./benchmarks/harness/exploit_http.py duplicate_content_length --compare-nginx
```

## Gates

```bash
./scripts/check-tier5-http-harness.sh
./scripts/check-tier5-nginx-src-audit.sh
./scripts/httpd-plan-gates.sh   # includes harness smoke when present
```
