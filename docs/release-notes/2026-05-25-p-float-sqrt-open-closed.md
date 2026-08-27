# P-float: close sqrt_open_bound

## Summary

Discharge `sqrt_open_bound.li` via `Li.Discharge.sqrt_open_bound_spec` and trusted `li_rt_sqrt_bound` axiom; retag manifest `prove_lean_ok`.

## Agent continuation

1. Read `docs/semantics/Discharge.lean`, `li-tests/tooling/discharge_sqrt_open_lean.sh`.
2. Run `./scripts/build.sh`, `./li-tests/tooling/discharge_sqrt_open_lean.sh`, `./li-tests/run_all.sh contracts_verify`.
3. Next: IEEE/libm proof; do not redo G-test-verify (#185 Done).
4. Blocked: G-hw ULP model sign-off for removing trusted axiom.

## Changed

| Path | Evidence |
|------|----------|
| `docs/semantics/Discharge.lean` | `Li.TrustedMath` + `sqrt_open_bound_spec_proved` |
| `compiler/verify/vc_{emit_lean,witness}.cpp` | semantic discharge emission |
| `li-tests/manifest.toml` | `prove_lean_ok` for `sqrt_open_bound.li` |
| `li-tests/tooling/discharge_sqrt_open_lean.sh` | zero open AutoVC + lake |

## Not changed

- G-test-verify manifest split (#185 merged).
- Full libm/IEEE proof (still **G-hw** axiomatic).

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| Breaking | N/A |
| Security | N/A |
| Performance | N/A |
| Downstream | N/A |
