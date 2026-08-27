# Release notes: proof database foundation (G-proof-db)

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** feat/proof-database-foundation  
**PH / REQ:** Doc-f, G-proof-db  

---

## Summary (one sentence)

Scaffolds the proof database: TOML schema, seed entries, `proof-db.py` CLI (`list`, `add-entry`, `verify-slice` stub), and **G-proof-db** Partial in provability-gaps.

## Agent continuation (required)

1. Read: `docs/verification/proof-database/README.md`, `schema.toml`.
2. Run: `python3 scripts/proof-db/proof-db.py verify-slice` and `list`.
3. Then: add rows when closing `contracts_verify` slices; wire `verify-slice --run-evidence`.
4. Blocked on: **G-lean** universal certificate — not this PR.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `docs/verification/proof-database/` | README, schema.toml, seed `entries/` | G-proof-db |
| `scripts/proof-db/proof-db.py` | list, add-entry, verify-slice | exit 0 |
| `docs/verification/provability-gaps.md` | **G-proof-db** Partial | register |
| `docs/superpowers/plans/2026-05-14-li-master-plan.md` | Doc-f + task row | master plan |

## Not changed (scope fence)

- Compiler, AutoVC, Discharge.lean behavior.
- **li-cursor-agents**, benchmarks dashboard.

## Breaking / Security / Performance / Downstream

N/A.

## CHANGELOG (Unreleased)

- **G-proof-db (Partial):** proof database foundation — this file.
