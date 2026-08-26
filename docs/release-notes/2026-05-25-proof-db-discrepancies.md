# 2026-05-25 — proof-db discrepancies

**Summary:** `compare_reference.py` diffs proof-db vs Discharge/ProofDB/trusted into `proof-database/DISCREPANCIES.md`.

## Agent continuation

1. Read `proof-database/DISCREPANCIES.md`, `proof-db/compiler/discrepancies-seed.json`
2. Run `python3 scripts/proof-db/compare_reference.py --write`
3. Update seeds when closing **P-float** / **G-par**

## Changed

- `scripts/proof-db/compare_reference.py`, `scripts/check-proof-db-discrepancies.sh`
- `proof-database/`, `proof-db/compiler/discrepancies-seed.json`, `proof-db/lemmas/P-float-sqrt-open-bound.toml`

## Not changed

AutoVC emit, `lic build` certificate.

## Breaking / Security / Performance / Downstream

N/A.
