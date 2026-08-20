# Float certificates: sqrt AutoVC fix, 2D matmul value cert, requires enforcement parity

**Date:** 2026-08-17
**Scope:** `compiler/verify/vc_emit_lean.cpp`, `compiler/verify/call_requires.{hpp,cpp}`, `compiler/types/typecheck.cpp`, `li-tests/contracts_verify/*`, `li-tests/manifest.toml`, `li-tests/tooling/discharge_sqrt_open_lean.sh`

## 1. `sqrt_open_bound.li` — valid Lean, still intentionally open

The sqrt `abs` lemma VC previously emitted broken AutoVC: the ensures def
`(x : Float) : Prop := Float.abs ((result * result) - x) < …` referenced
`result` without binding it, and the closing theorem placed `hreq` as an
argument. `lake build AutoVC` failed, and the grep-based open-goal check
mistook the malformed theorem for a proof.

Fix (`vc_emit_lean.cpp`): the ensures def now binds the return value
(`(x : Float) (result : Float) : Prop`), and **no closing theorem is emitted** —
the abs lemma stays an open obligation (Float lemmas pending) while AutoVC
compiles cleanly. The manifest outcome moved from `prove_lean_ok` to
`compile_open_ok`; `contracts_discharge_corpus.sh` still asserts the VC stays
open, and `discharge_sqrt_open_lean.sh` was updated to assert the new contract
(valid AutoVC + open goal). contracts_verify: **30/30**.

## 2. 2D float matmul value certificate

`li-tests/contracts_verify/linalg_mat2_float_value.li` (prove_lean_ok):

```li
def mat2_mul(A: array[2, array[2, float]], B: array[2, array[2, float]]) -> array[2, array[2, float]]
  requires true
  ensures result[0][0] == A[0][0] * B[0][0] + A[0][1] * B[1][0] and …
  decreases 0
=
  return A @ B
```

Filling A/B and calling `check_cell(C[0][0], 19.0)` … `C[1][1], 50.0`
discharges natively:

- **Nested element stores** — `array_index_const_key` now canonicalizes
  `a[i][j]` (and deeper) to `"a[i][j]"`, so `A[0][0] = 1.0` records per-cell
  float facts.
- **Per-cell call-eval** — `try_fold_call_to_const` walks the callee `ensures`
  conjunction, and each `result[i][j] == <expr>` conjunct is substituted,
  folded, and recorded as `C[i][j]`.

A negative guard (`linalg_mat2_float_wrong_value.li`, compile_fail E0304)
asserts `C[0][0] == 20.0` and fails with "19 == 20 not satisfied", proving the
fold is real.

## 3. Float requires enforcement parity (typechecker)

Wrong float call values now fail at typecheck with E0304 exactly like ints.
The typechecker (`typecheck.cpp`) tracks the same facts as the verify-side
collector, incrementally during its statement walk:

- `const_float_locals` for float var inits, ident copies, and
  `a[i] = c` / `a[i][j] = c` stores (int + float),
- counter-loop unrolling (`a[i] = 1.0` inside `while i < 4` records all four
  elements),
- call-assignment folding (`s = dot4_float(a, b)` → `s = 8.0` via the callee
  `ensures`), reusing `try_fold_call_to_const` with a proc-lookup instead of a
  full Module,
- `proof_facts()` now includes the float map.

The shared helpers (`array_index_const_key`, `subst_ident_lit`,
`simple_counter_loop`, lookup-based `try_fold_call_to_const`) are exported from
`li/call_requires.hpp` so the typechecker and the verify path can never drift.

## Verification

- contracts_verify **32/32** (incl. both new 2D tests), typecheck 8/8,
  math_linalg 26/26, effects 12/12, encapsulation 26/26, lexer_parser 7/7,
  runtime 4/4, smoke 4/4.
- `contracts_discharge_corpus.sh` ok; `discharge_sqrt_open_lean.sh` ok;
  prover parity `cpp=22 li=18 open=0`; lexer parity gate ok.
- Negative checks: `requires s == 9.0` (actual 8.0) → E0304; wrong matmul cell
  → E0304.
