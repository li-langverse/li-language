# Release notes: proof bypass via CLI flags only

## Summary

`lic build` and `lic verify` accept `--allow-open-vc` and `--no-lean-verify`; environment bypasses (`LI_ALLOW_OPEN_VC`, `LI_BUILD_VERIFY_LEAN*`) are ignored with a warning.

## Agent continuation

1. **Read** `compiler/types/include/li/proof_cli.hpp`, `compiler/lic/main.cpp`, `scripts/lean-verify-stub.sh`.
2. **Run** `lic build li-tests/contracts_verify/sqrt_open_bound.li` (must fail); `lic build --allow-open-vc …` (must pass when lake installed).
3. **Next** prove intentional open specimens; remove `sorry` from `mat2_at2_float_spec_proved`.
4. **Blocked on** P-float / loop implementation proofs for `sqrt_open_bound`, `linalg_dot4_int_loop_open`.

## Changed

| Path | Change |
|------|--------|
| `compiler/lic/main.cpp` | `--allow-open-vc`, `--no-lean-verify`; deprecate env |
| `compiler/types/include/li/proof_cli.hpp` | CLI-only proof flags |
| `scripts/lean-verify-stub.sh` | `--check-open-goals` arg (no env) |
| `li-tests/run_all.sh` | `verify_open_ok` uses `--allow-open-vc` |

## Not changed

- Intentional open proof corpus files (`sqrt_open_bound.li`, `linalg_dot4_int_loop_open.li`) — still open until proved.
- `Discharge.lean` `sorry` on `mat2_at2_float_spec_proved`.
- Httpd `proxy_*` weak-ensures name-prefix exemption in `typecheck.cpp`.

## Breaking

| Item | Migration |
|------|-----------|
| `LI_ALLOW_OPEN_VC=1 lic build …` | Use `lic build --allow-open-vc …` |
| `LI_BUILD_VERIFY_LEAN=0` | Use `lic build --no-lean-verify …` |

## Security / Performance / Downstream

N/A — CLI surface only; default gate unchanged (fail on open VCs).
