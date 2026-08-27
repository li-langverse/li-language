# Release notes: 2026-05-25 — proof-db-rebuild-pipeline

**Status:** Ready for review · **Repo:** lic · **PH:** Doc / 2f

## Summary

Lemma rebuild pipeline: load `docs/verification/proof-database/entries/`, targeted `lic build` per `li_specimen`, emit `data/proof-db/latest-report.{json,md}`.

## Agent continuation

1. Read `data/proof-db/README.md`
2. Run `./scripts/proof-db/rebuild_lemmas.sh` after `scripts/build.sh`
3. Update entries when closing **P-*** / **G-***
4. Blocked on: mandatory CI gate — none

## Changed

- `scripts/proof-db/rebuild_lemmas.{sh,py}`
- `docs/verification/proof-database/entries/contracts-verify.toml`
- `data/proof-db/`

## Not changed

AutoVC emit; `lic build --proof-db-report`.

## CHANGELOG

- **Proof DB lemma rebuild:** `rebuild_lemmas.sh` → `latest-report.{json,md}`.
