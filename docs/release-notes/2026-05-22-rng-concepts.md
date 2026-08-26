# rng-concepts — li-rng + OsRngUniform + Prng-on-TLS gates (2026-05-22)

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `cursor/httpd-plan-continue`  
**Plan todo:** `rng-concepts` → **completed**

---

## Summary (one sentence)

Ships `packages/li-rng` deterministic Prng surface, `prob_ensures` IV collision oracles under `OsRngUniform` / `PrngSeed`, and profile-gated `[server.rng]` validation for Prng-on-TLS.

## Changed

| Area | What | Evidence |
|------|------|----------|
| Package | `Prng`, `prng_seed`, `prng_next`, golden vectors | `li-tests/rng/prng_golden.li` |
| Prob | `iv_collision` under `OsRngUniform` / `PrngSeed` | `li-tests/rng/iv_collision_oracle.li` |
| Config | `httpd_rng.py` production reject / dev warn | `rng_prng_dev.toml`, `rng_prng_production.toml` |
| CI | `check-rng-concepts.sh` in `httpd-plan-gates.sh` | gate log |
| Spec | `docs/superpowers/specs/li-rng.md` | — |

## Not changed (scope fence)

- `li_rt_rng.c` getrandom seam — **G-runtime**
- `raises Rng` effect in compiler — **P0 follow-up**
- Tier F `BadRng` exploit injection — **`rng-exploit-suite` todo**

## Breaking changes

None (config reject only for new insecure `production` + `prng` + TLS combination).

## Security

Production profile rejects deterministic PRNG for TLS unless `allow_insecure_rng=true`. Dev/lab emits `insecure_rng_prng_tls` warning.

## Agent deliverable

- [x] Branch pushed and PR opened (not draft)
- [x] CI triggered on PR
- [x] Tests added / updated — paths: `li-tests/rng/`, `scripts/check-rng-concepts.sh`, `li-tests/config_desugar/*/rng_*.toml`
- [x] Bench evidence — N/A
- [x] Release notes / CHANGELOG if required by repo policy

## Live sites

Not refreshed (`SKIP_BENCH=1`).
