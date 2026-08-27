# li-rng — Prng, OsRng uniform contract, Prng-on-TLS profiles

## Packages

| Package | Role |
|---------|------|
| `li-rng` | Deterministic `Prng` (pure `prng_next` / `prng_seed`); future `li_rt_rng` CSPRNG seam |
| `li-prob` | `prob_ensures` MC discharge (`lic build --prob-check`) |

## Deterministic Prng

```nim
type Prng = object
  state: int

def prng_seed(p: var Prng, seed: int) -> unit
def prng_next(p: var Prng) -> int   # LCG state step (full `state`; `>>` when bitwise ops ship)
```

No `raises Rng` in user modules; only trusted runtime fill (when `li_rt_rng.c` lands).

## OsRng uniform contract

Lean: `docs/semantics/Probability.lean` — `axiom OsRngUniform`.

Li contracts:

```nim
prob_ensures iv_collision < ε
  given OsRngUniform
  samples N
```

Empirical evidence: PractRand + Tier F (`rng-exploit-suite` todo); MC via `scripts/prob_check.py`.

## Config — Prng on TLS (profile-gated)

```toml
profile = "dev"   # production default rejects prng+TLS

[server.rng]
mode = "prng"     # os | prng | sim | bad (bad = exploit harness only)
seed = 42
# allow_insecure_rng = true   # production escape hatch
```

| Profile | `mode = "prng"` + TLS |
|---------|------------------------|
| `dev` / `lab` / `prob-test` | Allowed — stderr warning `insecure_rng_prng_tls` |
| `production` | Rejected unless `allow_insecure_rng=true` |

Validator: `scripts/httpd_rng.py` (wired in `httpd_config.py` + `validate-httpd-config.py`).

## CI

`scripts/check-rng-concepts.sh` — `li-tests/rng/prng_golden.li`, `iv_collision_oracle.li`, config golden/reject TOMLs.
