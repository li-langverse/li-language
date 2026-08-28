# Release notes: post-#83 documentation sync

## Summary

After merging PR **#83** (2e/2f/H routing) and **#88** (master plan H M1 checkbox), align verification and httpd prerequisite docs with measured `main` results (16/16 `contracts_verify`, routing binary exit 0).

## Agent continuation

1. Read `docs/verification/proof-corpus-roadmap.md`, `docs/ecosystem/httpd-prerequisites.md`, master plan tracker § 2e–2f / H.
2. Run `./li-tests/run_all.sh contracts_verify` and `./li-tests/run_httpd_config.sh` on `main`.
3. Then: implement TOML route loader in Li (`m1-toml-desugar` in httpd plan); discharge refinement `ensures` in Lean (**P-refine**).
4. Blocked on: full kernel proof without Lean 4 + lake in environment; CI `build-and-test` baseline failures unrelated to contracts.

## Changed

| Path | Change |
|------|--------|
| `docs/verification/proof-corpus-roadmap.md` | Run results table → `main` 16/16 + httpd ok |
| `docs/ecosystem/httpd-prerequisites.md` | P0-lean → partial after #83 |
| `docs/superpowers/plans/2026-05-14-li-master-plan.md` | Phase table rows 2e, 2f, H |
| `docs/superpowers/plans/2026-05-16-li-httpd-plan.md` | Honest starting point; `w0-lean-gate`, `m1-routing-tests` in progress |
| `docs/release-notes/2026-05-20-phase-h-m1-routing-match.md` | Agent continuation (binary exit 0 landed in #83) |

## Not changed

- Compiler VC emission, witness, or codegen logic.
- `li-tests/manifest.toml` outcomes.
- TOML config desugar implementation.
- Benchmarks dashboard or `catalog.toml`.

## Breaking

N/A — documentation only.

## Security

N/A

## Performance

N/A

## Downstream

Agents should treat **2e–2f** as unblocked for M1 `.li` work at compile-gate level; full Lean certificate remains v2.
