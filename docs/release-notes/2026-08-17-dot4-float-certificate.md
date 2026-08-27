# Finished float dot4 certificate

**Date:** 2026-08-17
**Scope:** `compiler/verify/vc_witness.cpp`, `compiler/verify/vc_prove.{hpp,cpp}`, `compiler/verify/call_requires.{hpp,cpp}`, `compiler/verify/vc_emit_lean.cpp`, `li-tests/contracts_verify/linalg_dot4_float_closed.li`

## Summary

The fixed-size float dot is now a **finished certificate**:
`li-tests/contracts_verify/linalg_dot4_float_closed.li` builds under the strict
`prove_lean_ok` gate (no `--allow-open-vc`, zero open AutoVC goals) with all
three forms proved natively:

```li
def dot4_float(x: array[4, float], y: array[4, float]) -> float
  requires true
  ensures result == x[0] * y[0] + x[1] * y[1] + x[2] * y[2] + x[3] * y[3]
  decreases 0
=
  return x @ y
```

1. **Closed form** — `return x @ y` is witnessed equal to the expanded sum
   (new `@`-on-arrays witness, alongside the existing prelude `dot()` witness).
2. **Handwritten loop** — `acc = acc + x[i] * y[i]` with `while i < 4` is
   witnessed equal to the same sum. The loop witness previously hardcoded
   names (`a`, `b`, `acc`, `i`, bound 4); it now derives the array names from
   the spec terms and the accumulator/index names from the loop body, so it
   works for any parameter names (`x`/`y`, ...) and any accumulator name.
3. **Concrete value** — filling `a` with `1.0`, `b` with `2.0`, then
   `var s: float = dot4_float(a, b)` and `requires s == 8.0` at a helper call
   discharges. Three pieces make this work:
   - **Loop unrolling in fact collection** (`call_requires.cpp`): simple
     `while i < N` counter loops (known start, `i = i + 1` increment, no other
     writes to `i`, no break/continue/return, ≤ 64 iterations) are unrolled
     symbolically, so `a[i] = 1.0` records `a[0..3] = 1.0` per element.
   - **Float const facts** (`CallerProofFacts::const_float_locals`): float var
     inits, float ident copies, `a[i] = 1.0` stores, and float object fields.
   - **Call-assignment evaluation**: `s = dot4_float(a, b)` substitutes the
     callee's `ensures result == <expr>` with the actual args and constant-
     folds it with the accumulated facts via the new `fold_const_locals`
     (which collapses substituted literal arithmetic through the exported
     `fold_const`), recording `s = 8.0`. The requires-check fold path now
     compares float literals too (`expr_statically_true/false`).

## Files touched

- `compiler/verify/vc_prove.hpp/.cpp` — exported `FoldVal`/`fold_const` so the
  requires/witness layers share one constant folder.
- `compiler/verify/call_requires.hpp/.cpp` — `const_float_locals` facts,
  `fold_const_locals` + `fold_facts_expr`, float literal comparison in
  `expr_statically_true/false`, `try_fold_call_to_const`, `simple_counter_loop`
  unrolling, `collect_caller_proof_facts(caller, module)`.
- `compiler/verify/vc_witness.cpp` — generalized dot4 loop witness, new `@`
  matmul witness, float-aware `ident_return_matches_const_ensures` and
  `call_return_folds_to_ensures`.
- `compiler/verify/vc_emit_lean.cpp` — threads the `Module` into fact
  collection; call-site requires/refinement folding uses the float facts.

## Verification

- `linalg_dot4_float_closed.li` (new finished form) builds strictly, zero open
  AutoVC goals; `linalg_dot4_int_closed.li` and `linalg_dot4_int_loop_open.li`
  still pass (`prove_lean_ok`).
- `contracts_verify` suite: 29 pass / 1 fail — the only failure is
  `sqrt_open_bound.li`, the intentional `abs` float-lemma gap that
  `contracts_discharge_corpus.sh` explicitly asserts stays open (pre-existing).
- `typecheck` 8/8, `lexer_parser` 7/7, prover parity `cpp=22 li=18 open=0`,
  lexer parity gate ok (compiler changes are additive to the discharge layer).
- Negative check: `requires s == 9.0` against the folded `s == 8.0` leaves the
  (false) call-site VC open exactly like the pre-existing int call-eval
  behavior — requires enforcement at typecheck still covers direct literals and
  const locals (`chkf(8.0)` with `requires s == 9.0` fails with E0304).
