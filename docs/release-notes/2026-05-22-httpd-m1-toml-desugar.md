# Release notes: HTTPd M1 TOML desugar goldens

## Summary

Completes plan todo **m1-toml-desugar**: simple `[routes]` map desugar (prefix, prefix_strip, header extras), golden fixtures under `li-tests/config_desugar/`, and C/Python parity for `lic httpd explain-config`.

## Changes

| Area | Detail |
|------|--------|
| `scripts/httpd_config.py` | Stable route slugs for `/*` and `/**`; header extras in explain output |
| `runtime/li_rt_httpd.c` | Parse route-key header extras; explain prints `[key=value]`; slug aligns with Python |
| `li-tests/config_desugar/` | `route_patterns.toml` golden; `reject/bad_route_extra.toml` |
| `scripts/check-httpd-config-desugar.sh` | All `good/*.toml` vs `*.explained.golden` (C + Python) |
| `li-tests/run_httpd_config.sh` | Loops goldens + desugar check in CI gate |

## Test plan

```bash
./scripts/httpd-plan-gates.sh
./scripts/check-httpd-config-desugar.sh
lic httpd explain-config li-tests/config_desugar/good/route_patterns.toml
```

## Next

- **m1-core** — remaining M1 parser/LB/rate-limit Li surface
