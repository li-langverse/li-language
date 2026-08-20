# Layer 3: Self-hosted li prover reaches rewriting parity — `lic` proves in li

`2026-08-17`

Layer 2 (`vc_prove`) discharges theorem propositions natively in C++. Layer 3
ports the **rewriting half** of that engine into li itself
(`bootstrap/prover/main.li`), so `lic` can close the closed math corpus with a
compiler that is partly written in li. The linear-integer-arithmetic half
(Fourier-Motzkin) stays a compiler builtin, like `omega` in Lean.

## Self-hosted prover (`bootstrap/prover/main.li` → `build/prover/li_prover`)

`lic` serializes each non-axiom `theorem`/`lemma` proposition (int-encoded
pre-order tree) as one argv entry, runs the li prover binary, and reads one
verdict per line. The li prover implements:

- **Expansion** — products over sums (distributivity) with non-overlapping
  buffer regions, so `a * (b + c)` and `a * b + a * c` share one canonical form.
- **Canonicalization** — Sum-of-Products via AC normalization, constant
  folding, identity rules (`x + 0 = x`, `x * 1 = x`, `x - x = 0`, `x * 0 = 0`),
  with deterministic term/factor ordering.
- **Order reasoning** — reachability over assumed `<`/`<=` edges for
  `P -> Q` discharge.
- **Boolean simplification** — three-valued `and`/`or`/`not`/`->`.

On `proof-db/math/lemmas/ring_discharge.li` the li prover now closes **18 of 22**
propositions on its own (all ring/field/order/boolean identities); the 4 LIA
theorems (`add_lt_mono`, `add_lt_mono_sum`, `lt_asym`, `cell_index_range`) close
through the C++ builtin.

## Bridge + gate fixes

- **`compiler/verify/vc_li_prover.cpp`** — the prover binary lives at
  `build/prover/li_prover`; the bridge previously looked for `build/prover` (a
  directory), so the self-hosted prover silently fell back to open. Now it
  builds/checks the correct path.
- **`compiler/lic/main.cpp`** — the `lic build` / `lic verify` gate now treats a
  theorem as discharged when **either** engine closes it (C++ rewriting+LIA or
  the li prover). A theorem stays open only when both leave it open, so LIA
  theorems no longer fail the build just because the li prover alone cannot
  close them. `verify` reports `theorems_li_proved=` (li's own count) and
  `theorems_open=` (open in both).

## Verification surface

- `lic verify proof-db/math/lemmas/ring_discharge.li` →
  `theorems_proved=22 theorems_li_proved=18 theorems_open=0`.
- `lic build` succeeds on `ring_discharge.li` (manifest `verify_ok`).
- New tooling guard: `li-tests/tooling/check_li_prover_parity.sh` asserts the
  li prover closes ≥ 18 propositions and the combined gate is fully closed
  (`LI_PROVER_MIN` to raise once LIA is ported into li).

## Status

The rewriting engine is at parity (18/22). Porting the LIA half into li is
in progress; when it lands, `LI_PROVER_MIN=22` and the li prover closes the
whole corpus without the C++ builtin.
