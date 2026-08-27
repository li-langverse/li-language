# Layer 2: Native proof discharge — `lic` closes theorem/lemma propositions without Lean

`2026-08-14`

Layer 1 (li theorem syntax + ported math axioms) made the proof database
parse, typecheck, and build in li. Layer 2 makes `lic` **discharge** theorem
propositions itself, so the Lean round-trip is no longer required for the
closed math corpus.

## Native discharge engine (`compiler/verify/vc_prove.{hpp,cpp}`)

`discharge_theorems_natively(module)` runs every non-axiom `theorem`/`lemma`
proposition through a rewriting prover:

- **Arithmetic normalization** — expansion of products over sums
  (distributivity, including subtraction), AC normalization of `+` and `*`
  with constant folding, identity rules (`x + 0 = x`, `x * 1 = x`,
  `x - x = 0`, `x * 0 = 0`, `0 / b = 0`, `a / 1 = a`), and a shared
  Sum-of-Products canonical form so `a * (b + c)` and `a * b + a * c` are
  literally the same term.
- **Boolean simplification** — three-valued evaluation of `and` / `or` /
  `not` / `->` (`true` / `false` / unknown).
- **Order reasoning** — when discharging `P -> Q`, the conjuncts of `P` are
  assumed as facts; `<`/`<=`/`>`/`>=` goals then close by path reachability
  over those edges (transitivity), e.g. `(a < b and b < c) -> a < c`,
  and equalities follow from assumed order facts.

Idents are treated as opaque atoms: a proposition closes iff it is a
tautology of its atoms under these rewrites (a sound, if modest, base).

## Verification surface

- `lic verify` now prints `theorems_proved=` / `theorems_open=` and lists the
  open names (warnings only).
- `lic build` **fails** when a non-axiom theorem/lemma proposition is not
  discharged natively, with a hint to simplify the proposition, strengthen it
  into an `axiom`, or pass `--allow-open-vc` (documented dev/tests only).
  Axioms remain the trusted base and are counted separately.

## Proof-db corpus: `proof-db/math/lemmas/ring_discharge.li`

17 declarations the engine closes with zero Lean involvement — int ring
identities (comm/assoc/zero/one/cancel/distributivity), float field identities,
and order theorems (`order_transitive_int`, `order_transitive_le`,
`order_irreflexive`, `order_antisym_via_le`), plus boolean reasoning
(`not_not_eq`, `or_self`). `lic verify` reports `theorems_proved=17
theorems_open=0`, and the file is a `verify_ok` fixture so the closed set
cannot regress. `li-tests/prove_reject/theorem_open.li` asserts the gate the
other way: a genuinely open proposition (addition monotonicity) fails
`lic build`.

## Index-checker extension (tetris)

Two companion changes make runtime-computed array indices provable:

- **Index helpers** — `arr[helper(args)]` is accepted when the helper's
  `ensures` bound the result to `[0, array_size)` (e.g.
  `ensures result >= 0 and result < 200`). The caller trusts the helper
  contract; the helper's own ensures VC discharges separately.
  (`compiler/types/typecheck.cpp`)
- **Literal-range witness** — `def`/`extern` returns closed by a literal
  interval ensures (e.g. `in_bounds`'s `result >= 0 and result <= 1`).
  (`compiler/verify/vc_witness.cpp`)

Tetris (`examples/tetris/main.li`) dropped from ~18 errors (contracts +
E0201 index-policy) to a single open VC: `cell_index`'s range claim
`result >= 0 and result < 200`, which is mathematically unprovable without
requires on its raw `col`/`row` params (game invariant). The 8 E0201
"computed index" rejections are gone.

## Regression fixtures (`li-tests/manifest.toml`)

So the missing-contracts fix can't regress, the contract-covered files are now
`verify_ok` fixtures:

- `tier2_physics` — 20 benchmark kernels (`../benchmarks/tier2_physics/*/li/main.li`)
- `std_contracts` — `std/io/io.li`, `std/csv/csv.li`, `std/ui/ui.li`
- `proof_db_physics` — `conservation.li`, `momentum_invariant.li`
- `proof_db_math` — `ring_discharge.li`; `prove_reject` — `theorem_open.li`

All suites green: proof_db_math 3/3, proof_db_physics 2/2, tier2_physics
20/20, std_contracts 3/3, prove_reject 7/7, lexer_parser 7/7, typecheck 8/8,
integration 1/1, borrow 3/3, effects 12/12, contracts_verify 27/30 (the 3
failures are the pre-existing documented AutoVC/Lean emission gaps, untouched
by this change).
