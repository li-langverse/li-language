# Release notes: 2026-05-25 — proof-db-ci-gate

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** feat/proof-db-ci-gate  
**PH / REQ:** PH-2f  

## Summary (one sentence)

Adds a proof-database release gate that exports Lean theorem statuses to JSONL, pins `proof-db/baseline.jsonl`, and runs an advisory CI check after semantics lake and AutoVC open-goal enforcement.

## Agent continuation (required)

1. Read: `proof-db/README.md`, `scripts/export-proof-db.sh`, `scripts/check-proof-db.sh`.
2. Run: `./scripts/build.sh`; `lic build li-tests/modules/greeter/greeter.li -o /dev/null`; `LI_PROOF_DB_STRICT=0 ./scripts/check-proof-db.sh`.
3. Then: rerun `./scripts/export-proof-db.sh > proof-db/baseline.jsonl` when statuses change intentionally.
4. Blocked on: none.

## Changed (specific)

- `proof-db/` Lean bridge, four-row `index.json`, `baseline.jsonl`, README.
- `scripts/export-proof-db.sh`, `scripts/check-proof-db.sh` with `LI_PROOF_DB_STRICT` and `PROOF_DB_SKIP`.
- `scripts/ci.sh` and `scripts/check-master-plan-gates.sh` wiring after lake + AutoVC gate.
- `docs/verification/proof-corpus-roadmap.md` gate row.

## Not changed (scope fence)

- Physics and unrelated proof-db pipelines — not modified.
- `autovc_std_*` compiler emission — not wired.

## Breaking changes

None.

## Security

N/A — read-only scan and baseline diff.

## Performance

N/A.

## Downstream

N/A.
