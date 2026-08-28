# Release notes: 2026-05-25 — bench-verify-tier0-smoke

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** feat/fresh-bench-verify  
**PH / REQ:** Phase 5b harness (benchmarks plan)  
**Author:** agent

---

## Summary (one sentence)

Closes the benchmarks plan row for Li tier-0 `verify.py` + `lic build` with contract tests, harness docstrings, and CI wiring before full `bench.py --tier 0`.

## Agent continuation (required)

1. Read: `docs/superpowers/plans/2026-05-14-benchmarks-and-simulations.md` reference checklist (3 rows still open).
2. Run: `./scripts/build.sh` then `./scripts/check-bench-harness-contract.sh`.
3. Then: C++ path in `verify.py` (row 275); shared `params.toml` (row 273).
4. Blocked on: human merge; no self-merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| Harness | tier-0 `lic build` smokes | `PASS verify` ×3 |
| Tests | `test_harness_contract.py` | 6 tests OK |
| CI | contract gate before tier 0 | `scripts/ci.sh` |
| Plan | Li reference → `[x]` | benchmarks plan line 276 |

## Plan audit (before → after)

Open reference rows: **3** (was 4); Li `verify.py` row closed.

## Not changed (scope fence)

- C++ `verify.py` path, `params.toml` layout, tier 1–2 thresholds, `li-tests` manifest.

## Breaking / Security / Performance

N/A.

## Downstream

N/A.
