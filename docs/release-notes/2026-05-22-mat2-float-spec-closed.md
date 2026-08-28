# Close mat2_at2_float_spec (remove Discharge sorry)

## Summary

`Li.Discharge.mat2_at2_float_spec_proved` is now a closed `rfl` proof over `mat2_at2_eval`; AutoVC 2×2 `@` ensures no longer quantify over an unconstrained `result`.

## Agent continuation

1. **Read:** `docs/semantics/Discharge.lean`, `compiler/verify/vc_emit_lean.cpp` (mat2 discharge branch).
2. **Run:** `./li-tests/tooling/discharge_linalg_int_lean.sh`; `lake build` in `docs/semantics` (no `sorry` on Discharge).
3. **Next:** P-float `sqrt_open_bound.li`; HTTPd [#162](https://github.com/li-langverse/lic/pull/162) rebase on `main`.
4. **Blocked on:** proving MIR `ArrayMatMul2DF64` refines `mat2_at2_eval` (future **G-trust** / codegen witness).

## Changed

| Area | Path |
|------|------|
| Discharge | `docs/semantics/Discharge.lean` — `mat2_at2_eval`, proof without `sorry` |
| VC emit | `compiler/verify/vc_emit_lean.cpp` — ensures `Prop` uses `mat2_at2_eval A B` |
| Gaps | `docs/verification/provability-gaps.md` |

## Not changed

- `sqrt_open_bound.li` intentional open specimen.
- Tier-1 bench thresholds.

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A |
| **Security** | N/A |
| **Performance** | N/A |
| **Downstream** | N/A |
