# Release notes: P-loop — close int dot loop AutoVC (2f)

## Summary

`linalg_dot4_int_loop_open.li` now discharges like other P-linalg specimens: static `witness_dot4_int_loop` → closed AutoVC; semantic lemma `Li.Discharge.dot4_int_loop_eval_spec` in Lean.

## Agent continuation

1. **Read** `compiler/verify/vc_witness.cpp` (`witness_dot4_int_loop`), `docs/semantics/Discharge.lean`.
2. **Run** `./li-tests/tooling/discharge_linalg_int_lean.sh`; `lic build li-tests/contracts_verify/linalg_dot4_int_loop_open.li`.
3. **Next** P-float (`sqrt_open_bound`); remove `sorry` from `mat2_at2_float_spec_proved`.
4. **Blocked on** full MIR↔Lean equivalence for arbitrary loops (**G-trust**).

## Changed

| Path | Change |
|------|--------|
| `compiler/verify/vc_emit_lean.cpp` | Loop witness uses `Prop := True` + trivial `_proved` (same as closed dot) |
| `docs/semantics/Discharge.lean` | `dot4_loop_eval`, `dot4_int_loop_eval_spec` (rfl) |
| `li-tests/tooling/discharge_linalg_int_lean.sh` | Include loop specimen |
| `docs/verification/provability-gaps.md` | Loop dot no longer intentional open |
| `docs/language/contracts-and-proofs.md` | Sync implementation status |

## Not changed

- `sqrt_open_bound.li` still open (`verify_open_ok` / `--allow-open-vc`).
- `mat2_at2_float_spec_proved` still `sorry`.
- No general `while` loop prover in Lean.

## Breaking

N/A — stricter default: loop dot builds without `--allow-open-vc`.

## Security / Performance / Downstream

N/A.
