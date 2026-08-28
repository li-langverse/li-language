# w0-lean-gate — TLS/H2 runtime link + Lean CI (2026-05-23)

## Summary

Keeps **2e–2f** httpd proof gates green after M2 `li_rt_tls.c` / `li_rt_h2.c` symbols were wired from `li_rt_net.c`: manual C explain-config link matches `lic build` net closure; Lean workflow runs `check-httpd-lean-gate.sh`.

## Agent deliverable

- [x] `scripts/check-httpd-explain-config.sh` links `li_rt_tls.c`, `li_rt_h2.c`, `-ldl`
- [x] `.github/workflows/lean.yml` — `discharge_http_forward_lean.sh` + `check-httpd-lean-gate.sh`
- [x] Plan todo `w0-lean-gate` remains **completed**

## Tests

```bash
./scripts/check-httpd-lean-gate.sh
./scripts/check-httpd-explain-config.sh
HTTPD_GATES_SKIP_LIC_BUILD=0 ./scripts/httpd-plan-gates.sh
```

## Honesty

Composite `parse_request_smoke` still documents ≤8 open AutoVC goals with `--allow-open-vc`; closed slice is `http_parse_forward_closed.li`.
