# Release notes: 2026-05-25 — proof-db-math-axioms

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** branch `feat/proof-db-math-axioms`  
**PH / REQ:** PH-proof-db, G-math  
**Author:** agent

---

## Summary (one sentence)

Seed the classical **math** proof-database vertical: nine `M-AX-*` axioms (Peano, order, ℝ field), six `M-LM-*` lemmas including float add-comm discrepancy, TOML catalog + `proof-db.py verify-slice`.

## Agent continuation (required)

1. Read: `docs/verification/proof-database.md`, `proof-db/math/README.md`, `docs/verification/proof-database/entries/math-*.toml`.
2. Run: `python3 scripts/proof-db/proof-db.py verify-slice && python3 scripts/proof-db/proof-db.py list --field math`.
3. Then: wire math slice into CI advisory gate (`LI_PROOF_DB_STRICT`) when physics/compiler slices land; close `M-LM-FLOAT-ADD-COMM` via `P-float` numerics policy.
4. Blocked on: human numerics sign-off for float vs ℝ commutativity — or **none** for catalog-only merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| Schema | `entries_root`, required axiom/lemma fields, `proof_status` enum | `docs/verification/proof-database/schema.toml` |
| Catalog | 9 axioms + 6 lemmas, `release_pin = 2026-05-25`, `gap_id = G-math` | `docs/verification/proof-database/entries/math-*.toml` |
| CLI | `list`, `verify-slice` with `[[entry]]` parsing | `scripts/proof-db/proof-db.py` |
| Corpus | Lean stub + float specimen discrepancy | `proof-db/math/axioms/MathAxioms.lean`, `proof-db/math/lemmas/add_commutative.li` |
| Docs | Math hub + discrepancy policy | `docs/verification/proof-database.md`, `proof-db/math/discrepancy-policy.md` |

- `docs/verification/proof-database/schema.toml` — v1 schema roots and required columns.
- `docs/verification/proof-database/entries/math-axioms.toml` — `M-AX-*` → `MathAxioms.lean`.
- `docs/verification/proof-database/entries/math-lemmas.toml` — five `proved`, one `discrepancy` (`M-LM-FLOAT-ADD-COMM`, backlog `P-float`).
- `scripts/proof-db/proof-db.py` — local catalog verification before rebuild pipeline.

## Not changed (scope fence)

- Physics proof-db vertical (`P-AX-*`) — **not** in this slice.
- Compiler / AutoVC discharge wiring — **not** changed.
- `trusted.lean` — **no** new axioms.
- LLVM backend / tier benches — **not** touched.

## Breaking changes

None.

## Security

N/A — catalog and documentation only; no runtime surface change.

## Performance

N/A — no benchmark or codegen impact.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit / lis | N/A |
| benchmarks | Optional future row when math slice joins dashboard |

## CHANGELOG entry (paste into Unreleased)

```markdown
### Added
- Proof database math vertical: nine `M-AX-*` axioms, six `M-LM-*` lemmas, `proof-db.py verify-slice` (G-math).
```
