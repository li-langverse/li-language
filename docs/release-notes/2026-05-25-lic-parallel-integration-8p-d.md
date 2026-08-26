# Wave 2 integrator — explicit parallel CI flags (8p-d)

## Summary

`scripts/ci.sh` drives manifest `run_all` with explicit `-j8 --max-memory=8192` instead of exporting `LI_TEST_JOBS`; baseline doc records `wall_s` / `peak_rss` rows (not measured yet).

## Agent continuation

1. Read `scripts/ci.sh` `RUN_ALL_FLAGS` and `docs/ecosystem/lic-ecosystem-baseline.md` §8p throughput.
2. Run `./scripts/build.sh`; smoke `./li-tests/run_all.sh -j2 --max-memory=8192 math_syntax`.
3. After [#205](https://github.com/li-langverse/lic/pull/205) merges: re-run `local-ci.sh`, fill baseline `wall_s` / `peak_rss`.
4. Blocked: WP3 `lic check --workspace --jobs=8` stage until workspace check lands.

## Changed

| Path | Evidence |
|------|----------|
| `scripts/ci.sh` | `RUN_ALL_FLAGS=(-j8 --max-memory=8192)` on manifest suites |
| `li-tests/run_all.sh` | `--max-memory=MB` forwarded to `lic build` |
| `docs/ecosystem/lic-ecosystem-baseline.md` | `wall_s` / `peak_rss` table + 8p-d checklist |

## Not changed

- Proof gates, `trusted.lean`, tier physics benches, `ci-security.sh` suite invocations
- `lic build --jobs` frontend parallelism (8p-c)
- WP3 workspace check (comment-only skip in `ci.sh`)

## Breaking / Security / Performance / Downstream

| Topic | Status |
|-------|--------|
| **Breaking** | N/A |
| **Security** | N/A — same suites/outcomes |
| **Performance** | CI manifest phases use 8 workers + 8 GiB cap via flags |
| **Downstream** | Stacks on PR [#205](https://github.com/li-langverse/lic/pull/205) |
