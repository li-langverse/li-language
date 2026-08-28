# Release notes: 2026-05-25 — proof-db-physics-axioms

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/proof-db-physics-axioms`  
**PH / REQ:** Doc / 2f classical physics baseline (**P-physics**, **G-physics**)

---

## Summary (one sentence)

Seeds the proof database **classical physics** vertical with seven `P-AX-*` axioms, three `P-LM-*` lemmas (two proved scalar stubs in `Discharge.lean`, one open **modeling_gap**), tier-2 bench cross-refs, and tier-0 `li_tests` smokes.

## Agent continuation (required)

1. Read: [proof-database.md](../verification/proof-database.md), [proof-database/README.md](../verification/proof-database/README.md), [provability-gaps.md](../verification/provability-gaps.md) (**G-physics**, **G-proof-db**)
2. Run: `python3 scripts/proof-db/proof-db.py verify-slice` (when present); `cd docs/semantics && lake build` for `Discharge.lean` physics lemmas; tier-0 `md_energy_single_step.li`, `three_body_invariants.li`
3. Then: close **P-AX-MECH-002** / **P-AX-DIM-*** via `contracts_verify` + real kernel `ensures`; keep **modeling_gap** on extern tier-2 until specs export
4. Blocked on: universal tier-2 VC emit from `lic build` — not required for this doc/TOML seed PR

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| Catalog | `docs/verification/proof-database/entries/physics-{mechanics,conservation,dimensions,lemmas}.toml` | 7 axioms + 3 lemmas; pin `a9542bfc` |
| Index | `docs/verification/proof-database.md`, `proof-database/README.md` | `proof_gap` vs `modeling_gap` taxonomy |
| Lean | `docs/semantics/Discharge.lean` — `kinetic_energy_def_consistent`, `linear_momentum_linear_stub`, `force_equals_mass_accel_stub`, `dimensional_homogeneity_placeholder` | `lake build` in semantics job |
| Gaps | [provability-gaps.md](../verification/provability-gaps.md) — **G-physics**, **G-proof-db** register rows | links `entries/physics-*.toml` |
| Roadmap | [proof-corpus-roadmap.md](../verification/proof-corpus-roadmap.md) — **P-physics** backlog row | — |

## Not changed (scope fence)

- New `proof-db/` manifest or check scripts — **not** added in this doc/TOML slice (pre-existing `proof-db/physics/` specimens unchanged)  
- `vc_emit_lean.cpp` / AutoVC names for tier-2 physics drivers — **not** wired  
- `li-tests/manifest.toml` — no new suite row  
- `trusted.lean` — no new runtime axioms  

## Breaking changes

None.

## Security

N/A — documentation, TOML catalog, and Lean discharge stubs only.

## Performance

N/A — tier-2 bench refs are cross-links only; no harness threshold changes.

## Downstream

N/A.

## CHANGELOG entry (paste into Unreleased)

- **P-physics proof database:** `docs/verification/proof-database/entries/physics-*.toml` (`P-AX-*`, `P-LM-*`); tier-2 bench refs; scalar lemmas in `Discharge.lean` — `docs/release-notes/2026-05-25-proof-db-physics-axioms.md`.
