# Phase completion snapshot (2026-05-20)

**Merged:** PR **#83** (`cursor/refinement-call-check-57b4` → `main`).

## Status vs master plan

| Phase | Status | Notes |
|-------|--------|-------|
| **2e — Contracts + refinements** | **Merged** | E0304 call-site `requires`, E0305 refinements, VC emit + proof corpus |
| **2f — Lean in `lic build`** | **Partial on main** | Open-VC gate on build; `contracts_verify/` green; `sqrt_open_bound` intentionally open; Lean kernel optional via `LI_BUILD_VERIFY_LEAN` |
| **Phase H M1 routing** | **Partial on main** | `match_routes.li` exits 0; Python + Li oracles in `run_httpd_config.sh` |
| **Docs** | **Merged** | `llvm-abi.md`, `proof-corpus-roadmap.md`, `refinement-types.md` |

## Agent continuation

1. Read [proof-corpus-roadmap.md](../verification/proof-corpus-roadmap.md) and [provability-gaps.md](../verification/provability-gaps.md) (**G-lean**, **G-vc**).
2. Run `./li-tests/run_all.sh contracts_verify` and `./li-tests/run_httpd_config.sh`.
3. Next: float `abs` lemmas (`sqrt_open_bound`); TOML route table in Li; structured `disjoint=` (**7d-c**).
4. Close superseded draft PRs **#77**, **#78** if still open.
