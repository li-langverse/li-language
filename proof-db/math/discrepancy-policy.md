# Math proof-db discrepancy policy

When a **Li specimen** contract disagrees with the **Lean axiom layer** (or textbook reference), set `proof_status = discrepancy` in the proof-database TOML — do **not** treat it as a compiler bug or add `trusted.lean` axioms to silence it.

## M-LM-FLOAT-ADD-COMM

| Layer | Expectation |
|-------|-------------|
| Lean | `Li.ProofDb.Math.real_add_comm` on `ℝ` |
| Li specimen | [lemmas/add_commutative.li](lemmas/add_commutative.li) uses `float` and `return a + b` under `ensures result == b + a` |

**Backlog:** `P-float` — float numerics vs real axioms. Triage via provability gap `G-math`; rebuild pipeline may report open goals without implying user-code wrongness.

## Agent rules

1. Record discrepancy in TOML + release notes; keep specimen path in `li_specimen`.
2. Do not downgrade to `open` without human sign-off on numerics policy.
3. Prefer new backlog ids (`P-*`) over silent catalog edits.
