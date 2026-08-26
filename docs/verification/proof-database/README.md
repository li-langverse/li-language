# Proof database entries (`docs/verification/proof-database/`)

**Audience:** agents extending **P-physics** and tier-2 bench oracles without weakening discharge honesty.

**Index:** [proof-database.md](../proof-database.md) · [Provability gaps](../provability-gaps.md) (**G-physics**, **G-proof-db**) · [Proof corpus roadmap](../proof-corpus-roadmap.md) · **Schema:** [schema.toml](schema.toml)

## Discrepancy taxonomy (`gap_kind`)

| `gap_kind` | Meaning | Typical action |
|------------|---------|----------------|
| **`proof_gap`** | Statement is in scope for Lean/`contracts_verify`; specimen or `Discharge.lean` stub exists but kernel discharge is incomplete | Add lemma, wire `li_specimen`, run `discharge_*_lean.sh` |
| **`modeling_gap`** | Tier-2 `extern` or numeric driver uses weak `ensures` / wrong abstraction vs textbook claim | Fix kernel spec or bench oracle first; do **not** mark `proved` on `ensures true` alone |
| **`none`** | Row matches `Discharge.lean` or closed specimen | Refresh `last_verified_lic_commit` on compiler change |

`proof_status = discrepancy` (schema) means catalog vs spec/Lean/MIR disagree — triage before blaming the compiler.

## Layout

| Path | Role |
|------|------|
| [entries/physics-mechanics.toml](entries/physics-mechanics.toml) | `P-AX-MECH-001` … `003` |
| [entries/physics-conservation.toml](entries/physics-conservation.toml) | `P-AX-CONS-001` … `002` |
| [entries/physics-dimensions.toml](entries/physics-dimensions.toml) | `P-AX-DIM-001` … `002` |
| [entries/physics-lemmas.toml](entries/physics-lemmas.toml) | `P-LM-*` |

## Tier-2 bench refs

| Bench id | Path under `lic` |
|----------|------------------|
| `three_body` | `benchmarks/tier2_physics/three_body/` |
| `nbody_gravity` | `benchmarks/tier2_physics/nbody_gravity/` |
| `md_lennard_jones` | `benchmarks/tier2_physics/md_lennard_jones/` |

**Harness:** `benchmarks/harness/bench.py` → `TIER2_BENCHES`. **Org catalog:** `li-langverse/benchmarks` `catalog.toml`.

## Agent workflow

1. Read [proof-database.md](../proof-database.md) and the relevant `entries/physics-*.toml`.
2. Edit `docs/semantics/Discharge.lean` only for scalar lemmas; keep **modeling_gap** on axiom rows until kernels export real `ensures`.
3. Run `python3 scripts/proof-db/proof-db.py verify-slice` when CLI is on the branch.
4. Update **G-physics** in [provability-gaps.md](../provability-gaps.md) in the same PR.
