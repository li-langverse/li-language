# proof-db/math

Classical math vertical for the Li proof database.

| Area | Path | Catalog ids | Li layer |
|------|------|-------------|----------|
| Peano + order axioms (li) | [axioms/peano_order.li](axioms/peano_order.li) | `M-AX-PEANO-*`, `M-AX-ORDER-*` | `axiom` declarations — `peano_zero_not_succ`, `peano_succ_injective`, `order_trichotomy_nat`, `order_antisym` |
| ℝ field axioms (li) | [axioms/reals_field.li](axioms/reals_field.li) | `M-AX-REAL-*` | `axiom` declarations — `real_add_comm`, `real_add_assoc`, `real_mul_distrib`, `real_mul_one` |
| Lean mirror (export) | [axioms/MathAxioms.lean](axioms/MathAxioms.lean) | `Li.ProofDb.Math.*` | Same axioms kept in Lean for the AutoVC/lake pipeline |
| Lemma specimens | [lemmas/](lemmas/) | `M-LM-*` | Program specimens; `lemma` discharge is Layer 2 |

The li files are the source of truth for the **first-order** axiom layer; `lic`
parses and typechecks them natively (`lic check proof-db/math/axioms/peano_order.li`).
`peano_induction` (M-AX-PEANO-IND) is second-order and currently stays Lean-only.

**TOML source of truth:** `docs/verification/proof-database/entries/math-*.toml`.

```bash
python3 scripts/proof-db/proof-db.py list --field math
python3 scripts/proof-db/proof-db.py verify-slice
```
