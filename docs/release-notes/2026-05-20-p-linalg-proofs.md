# P-linalg — mathematical proof corpus (Phase 2f partial)

## Summary

Adds **P-linalg** contract specimens: fixed-size int dot/sum and matmul-entry postconditions with **fully discharged** AutoVC, plus an intentional **open** loop-dot VC for future Lean lemmas.

## Agent continuation

1. **Read** `li-tests/contracts_verify/linalg_dot4_int_closed.li`, `linalg_dot4_int_loop_open.li`, `docs/semantics/Discharge.lean`.
2. **Run** `./li-tests/tooling/discharge_linalg_int_lean.sh` and `./li-tests/run_all.sh contracts_verify` (**22** pass).
3. **Next** — prove `dot4_int_loop` ≡ closed form in `Discharge.lean`; emit real Props for `vec3_dot`-style float returns; fix nested `array[M, array[K,T]]` **CallProc** codegen (**G-math**).
4. **Blocked on** `Core.lean` array/loop semantics for loop↔formula proofs.

## Changed

| Path | Note |
|------|------|
| `li-tests/contracts_verify/linalg_dot4_int_closed.li` | Static witness, zero open goals |
| `li-tests/contracts_verify/linalg_sum4_int_closed.li` | Same |
| `li-tests/contracts_verify/linalg_mat2_entry00_int_closed.li` | Scalar matmul entry (avoids 2d array CallProc) |
| `li-tests/contracts_verify/linalg_dot4_int_loop_open.li` | Real Lean `Prop`, `verify_open_ok` |
| `li-tests/tooling/discharge_linalg_int_lean.sh` | Wired into `contracts_discharge_corpus.sh` |
| `docs/semantics/Discharge.lean` | `dot4_int_spec`, `mat2_entry00_int_spec` |
| `docs/verification/proof-corpus-roadmap.md`, `provability-gaps.md` | **G-math** / **P-linalg** rows |

## Not changed

- `lic build` default Lean kernel gate; `vec3_dot` float ensures still opaque stub.
- Tier-1 bench thresholds; `@` on nested float arrays in proc params (codegen gap).

## Breaking

N/A.

## Security

N/A.

## Performance

N/A.

## Downstream

- **benchmarks** ingest unchanged; proof honesty for math surface docs.
