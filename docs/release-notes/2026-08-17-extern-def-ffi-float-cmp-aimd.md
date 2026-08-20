# 2026-08-17 — `extern def` FFI, float-comparison codegen fix, and the `li-aimd` package

## `extern def` becomes canonical for FFI

`proc` is legacy; the last remaining holdout was `extern proc`, which the parser
required for foreign-function declarations. That special case is gone:

- **Parser** (`compiler/parser/parser.cpp`) accepts `extern def` and rejects `extern proc`
  with the same "expected `def`" path used for bare `proc`.
- **Typechecker** (`compiler/types/typecheck.cpp`) error messages now reference `extern def`.
- **Migrator** (`scripts/migrate-proc-to-def.py`) converts `extern proc` → `extern def`
  and skips `li-tests/` so negative/legacy fixtures are not rewritten.
- **Checker** (`scripts/check-li-def-syntax.sh`) flags both bare `proc` and `extern proc`.
- **Regression test** `li-tests/encapsulation/extern_proc_syntax_rejected.li` asserts
  `extern proc` is rejected (alongside the existing `proc_syntax_rejected.li`).

All repository `.li` sources were migrated (`extern proc` no longer appears outside the
rejection fixture).

## Float-comparison codegen fix

Runtime float comparisons were miscompiled. A comparison like `a >= 0.9` is not an
arithmetic binop, so the MIR lowering classified it as an integer operation
(`BinOpInt`), and loading a float operand through `load_int` applied `FPToSI` first.
Every operand in `(0, 1)` truncated to `0`, so `0.9 <= 0.8` and `0.0 >= 0.1` both
evaluated true.

The fix:

- `compiler/mir/lower.cpp` routes comparison binops with a float operand through
  `BinOpFloat` (without marking the result as a float local).
- `compiler/codegen/emit.cpp` `emit_fbinop` now emits `FCmp` + `zext` for
  `Lt/Le/Gt/Ge/Eq/Ne`, and `BinOpFloat` stores comparison results as i32.
- A new `load_as_f64` helper widens int operands with `SIToFP`, so mixed
  `float`/`int` comparisons (e.g. `x > 2`) remain correct.

This was the blocker that made the `li-aimd` oracle/batch smokes fail at runtime;
both now pass.

## Native VC engine: numeric constant folding

The native discharge engine folded only bare numeric literals, so a contract like
`ensures result == 3.0` did not close against `return 1.0 + 1.0 + 1.0` — the
witness layer compared the return expression against the literal syntactically,
and `1.0 + 1.0 + 1.0` has a different shape than `3.0`.

- `compiler/verify/vc_prove.cpp` adds `fold_const`, which reduces `+ - * /` over
  numeric literals to a single value: exact int64 arithmetic for pure-integer
  expressions (overflow and non-exact integer division stay unfolded, so a fold
  is never unsound) and IEEE double arithmetic once a float literal is involved
  (division by zero stays unfolded).
- `eval_cmp` uses `fold_const` before canonicalization, so `1.0 + 1.0 + 1.0 == 3.0`
  (and the int analogue `1 + 1 + 1 == 3`) close directly.
- `compiler/verify/vc_witness.cpp` closes `ensures result == <const>` against a
  constant return expression via the new `fold_numeric_equal` helper.
- `compiler/verify/call_requires.cpp` now records constant array-element stores
  (`a[i] = c`) alongside const locals/object fields, and `fold_const_int_locals`
  folds `a[c]` back to its stored value. This lets the witness layer close
  `return read_at(a, 6)` against `ensures result == 7` by substituting
  `read_at`'s `ensures result == a[i]`, folding `a[6]` to `7`.

`li-tests/physics/golden_positions_sum.li` now discharges under strict `lic build`
(no `--allow-open-vc`, no Lean), and `contracts_verify/bounds_refinement_release_ok.li`
also discharges (its `result == 7` closes against `read_at(a, 6)`).

## `li-aimd` standalone package

A new `packages/li-aimd` package (its own package, not nested under `li-chem` or
`li-sim`) with:

- `[dependencies]` on `li-chem` and `li-sim` and `import_name = "aimd"`.
- `src/lib.li`: DFT-coupled MD energy-drift oracle, harmonic thermostat, grand-canonical
  (SHE) charge-neutrality oracle, and headless batch runner (`AimdBatchResult`).
- `li-tests/smoke/`: `builds.li` (`verify_ok`), `aimd_oracle_smoke.li` and
  `aimd_batch_smoke.li` (`compile_open_ok`; runtime assertions verified by running the
  binaries).
- Registered as a workspace member and in the monorepo `li-tests/manifest.toml`
  (`smoke` suite).

## Verification

- `lic verify proof-db/math/lemmas/ring_discharge.li`: `theorems_proved=22
  theorems_li_proved=18 theorems_open=0`.
- `check_li_prover_parity.sh`: `cpp=22 li=18 open=0`.
- `li-tests/physics/golden_positions_sum.li` discharges strictly (the `result == 3.0`
  ensures closes against `return 1.0 + 1.0 + 1.0`), and the physics suite is 3/3.- Suites green: `smoke` (4/4, incl. the new AIMD smokes), `math_linalg` (26),
  `runtime` (4), `codegen` (1), `simd` (2), `proof_db_math` (3), `encapsulation` (26),
  `lexer_parser` (7), `typecheck` (8), `prove_reject` (7). Pre-existing Lean/AutoVC
  typeclass gaps in `composable` (10) and `contracts_verify` (2) are unchanged.
- `verify-math-physics-goldens.sh` passes end-to-end (runtime prints `3`).
