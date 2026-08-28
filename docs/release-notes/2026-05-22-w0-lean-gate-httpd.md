# w0-lean-gate — httpd VC+Lean path (2026-05-22)

## Summary

Unblocks spec-first **li-httpd** work on the proof pipeline: runtime link fix for `li_rt_log`, callee-ensures inheritance witness for direct extern forwards, closed `http_parse_forward_closed.li` corpus specimen, and `check-httpd-lean-gate.sh` wired into `httpd-plan-gates.sh`.

## Agent deliverable

- [x] `scripts/check-httpd-lean-gate.sh` + `HTTPD_LEAN_GATE_MAX_OPEN` budget (default 8)
- [x] `li-tests/contracts_verify/http_parse_forward_closed.li` — zero open AutoVC + lake
- [x] `compiler/verify/vc_witness.cpp` — inherit callee `ensures` on `return callee(...)`
- [x] `compiler/mir/mir_runtime_link.cpp` — link `li_rt_log.c` when `li_rt_net.c` is linked
- [x] Plan todo `w0-lean-gate` → **completed** (composite smokes still document ≤8 open goals)

## Tests

```bash
./scripts/check-httpd-lean-gate.sh
./li-tests/tooling/discharge_http_forward_lean.sh
HTTPD_GATES_SKIP_LIC_BUILD=0 ./scripts/httpd-plan-gates.sh
./li-tests/run_all.sh contracts_verify
```

## Honesty

- `parse_request_smoke` / full `li-http` import still have documented open VCs (if-chains, `result == 24` oracle, seam LB extern) unless `--allow-open-vc`.
- Full Lean kernel discharge on every `lic build` remains **G-lean** partial; see [provability-gaps.md](../verification/provability-gaps.md).

## Live sites

Not refreshed (`SKIP_BENCH=1` — proof/CI only).
