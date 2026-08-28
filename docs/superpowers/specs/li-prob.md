# li-prob — probabilistic Hoare (`prob_ensures`)

## Surface

```nim
prob_ensures <event> < ε
  given OsRngUniform | PrngSeed | BadRng
  samples <N>
```

- Parsed by `lic` as `ContractKind::ProbEnsures` (sibling to `requires` / `ensures`).
- Discharged at build time with `lic build --prob-check` → `scripts/prob_check.py`.
- Certificate: `build/generated/prob_check.json` (empirical `p_upper`, confidence).

## Events (MC harness)

| Event | Model |
|-------|--------|
| `duplicate_draw` | Birthday-style collision in `samples` draws from 2³² space |
| `iv_collision` / `iv_collision(...)` | 96-bit IV space collision rate |
| `always_false` | Regression (P=0) |

## Lean

`docs/semantics/Probability.lean` — `OsRngUniform`, `PrngSeed`, `ProbCheckCert` stub (Mathlib measure theory = G-lean).

## CI

`scripts/check-prob-hoare.sh` — `li-tests/prob/collision_oracle.li` + `--prob-check`.

## RNG hypotheses

See `docs/superpowers/specs/li-rng.md` — `given OsRngUniform`, `given PrngSeed`, `given BadRng` (Tier F).
