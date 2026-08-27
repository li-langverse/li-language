# Release notes: proof-db gap2 catalog slice (G-proof-db)

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** feat/gap2-proofdb  
**PH / REQ:** Doc-f, G-proof-db  

## Summary (one sentence)

Registers three `proof-db/math/lemmas` catalog targets (`L-MATH-*` → `M-LM-LMATH-*`) in `math-lemmas.toml` and adds `scripts/proof-db-gap2-report.sh` for slice coverage reporting.

## Agent continuation (required)

1. Read: `proof-db/math/lemmas/catalog.json`, `docs/verification/proof-database/entries/math-lemmas.toml`, `data/proof-db/gap2-report.md`.
2. Run: `./scripts/proof-db/rebuild_lemmas.sh --validate-only`; `./scripts/proof-db-gap2-report.sh`.
3. Then: close open rows via Lean discharge; reconcile with open PR `feat/gap-close-proof-db` if overlapping.
4. Blocked on: **G-lean** universal certificate — not this PR.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `math-lemmas.toml` | +3 `M-LM-LMATH-*` open lemmas (`catalog_id=L-MATH-*`) | validate-only → 38 entries |
| `scripts/proof-db-gap2-report.sh` | Catalog ↔ TOML gap2 report | `data/proof-db/gap2-report.md` |
| `scripts/proof-db/_gap2_report.py` | Report generator helper | invoked by shell wrapper |

## Not changed (scope fence)

- `lic` compiler, AutoVC emit, `Discharge.lean` proof bodies.
- `proof-db/index.json` Lean bridge (five `std_*` lemmas).
- **li-cursor-agents**, benchmarks dashboard, **lis** tier5.

## Breaking / Security / Performance / Downstream

N/A — proof-db registry and reporting only.
