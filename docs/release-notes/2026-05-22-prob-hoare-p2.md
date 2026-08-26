# prob-hoare P2 — probabilistic Hoare + lic build --prob-check (2026-05-22)

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `cursor/httpd-plan-continue`  
**Plan todo:** `prob-hoare` → **completed**

---

## Summary (one sentence)

Ships `prob_ensures` contracts, Monte Carlo discharge via `lic build --prob-check`, Lean `Probability.lean` measure stub, and `check-prob-hoare.sh` in httpd plan gates.

## Agent continuation (required)

1. Read: `docs/superpowers/specs/li-prob.md`, `li-tests/prob/collision_oracle.li`, `scripts/prob_check.py`
2. Run: `./scripts/check-prob-hoare.sh` and `./scripts/httpd-plan-gates.sh`
3. ~~`rng-concepts`~~ — completed on `cursor/httpd-plan-continue` (`docs/release-notes/2026-05-22-rng-concepts.md`)
4. Blocked on: **none** for P2 slice; full Mathlib `ProbabilityTheory` remains G-lean

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| Parser | `prob_ensures` + optional `given` / `samples` | `lic check li-tests/prob/collision_oracle.li` |
| CLI | `lic build --prob-check` → `scripts/prob_check.py` | `build/generated/prob_check.json` |
| MC | Hoeffding upper bound on named events | `duplicate_draw`, `iv_collision` |
| Lean | `docs/semantics/Probability.lean` + lake lib | `lake build Probability` (optional in gate) |
| Package | `packages/li-prob` version helpers | `li_prob_version() == 1` |
| CI | `scripts/check-prob-hoare.sh` wired in `httpd-plan-gates.sh` | gate log |

## Not changed (scope fence)

- TLS/RNG runtime (`li-rng` CSPRNG seam) — **not** in this PR (`rng-concepts` follow-up)
- Tier F `BadRng` exploit injection — **not** wired to MC failure yet
- Full Lean discharge of `prob_ensures` without MC — **G-lean**

## Breaking changes

None.

## Security

N/A — contract syntax + statistical gate only; no production TLS behavior change.

## Performance

N/A — PR CI uses 8k MC samples; override via `samples` clause or `LI_PROB_CHECK_DELTA`.

## Downstream

| Repo | Action |
|------|--------|
| lip / lit | N/A until `prob_ensures` in published toolchain notes |

## CHANGELOG entry (paste into Unreleased)

- **prob-hoare (P2):** `prob_ensures`, `lic build --prob-check`, MC certificate, `check-prob-hoare.sh` — `docs/release-notes/2026-05-22-prob-hoare-p2.md`.

## Agent deliverable

- [x] Branch pushed and PR opened (not draft)
- [x] CI triggered on PR
- [x] Tests added / updated — paths: `li-tests/prob/collision_oracle.li`, `scripts/check-prob-hoare.sh`
- [x] Bench evidence — N/A (proof/CI only)
- [x] Release notes / CHANGELOG if required by repo policy

## Live sites

Not refreshed (`SKIP_BENCH=1` — no benchmark CSV change).
