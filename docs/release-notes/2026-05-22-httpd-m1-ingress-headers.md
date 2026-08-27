# Release notes: httpd M1 ingress header allowlist

**PR:** #173

## Summary

Route-key header extras (e.g. `POST /v1/* x-model=gpt-4`) must use the M1 ingress
allowlist from the httpd plan. Hop-by-hop and `x-upstream-*` / `x-route-*` names are rejected at desugar time.

## Test plan

```bash
export PYTHONPATH=scripts
python3 scripts/httpd_config.py li-tests/config_desugar/reject/forbidden_ingress_header.toml && exit 1 || true
python3 scripts/httpd_config.py li-tests/config_desugar/good/route_patterns.toml
HTTPD_GATES_SKIP_LIC_BUILD=1 ./scripts/httpd-plan-gates.sh
```
