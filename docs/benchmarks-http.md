# HTTP benchmarks (tier5_http)

Plan: [2026-05-16-li-httpd-plan.md](superpowers/plans/2026-05-16-li-httpd-plan.md) · Physics bench plan: [2026-05-14-benchmarks-and-simulations.md](superpowers/plans/2026-05-14-benchmarks-and-simulations.md)

## Principle

Users add scenarios by editing **TOML only** — not Python. Harnesses merge layers and run verify-before-timing.

## TOML layers

| File | Purpose |
|------|---------|
| `benchmarks/tier5_http/defaults.toml` | Workers, ports, wrk defaults, fixture sizes |
| `benchmarks/tier5_http/suite.toml` | Profiles (`ci`, `baseline`) and include/exclude |
| `benchmarks/tier5_http/scenarios/<name>/bench.toml` | Server, verify, load, metrics |

Merge order: `defaults.toml` ← `scenarios/*/bench.toml` ← `--set key=value`.

## CLI

```bash
./benchmarks/harness/bench_http.py --profile ci
./benchmarks/harness/verify_http.py --profile ci --stub
./benchmarks/harness/bench_http.py static_small --set load.connections=500
```

See `benchmarks/tier5_http/README.md` and `params_schema.toml` for keys.

## Fairness

Same URL set, body sizes, worker model, and TLS labels across `lang` rows. Document `variant=` when parity is incomplete. CI uses `timing = false` — no flaky wrk in PR gates.

## CSV

Same columns as `benchmarks/harness/bench.py`: `benchmark, lang, variant, threads, metric, value, unit, git_sha, cpu_model, flags`.

Optional `benchmarks/results/verify_http.csv` from `verify_http.py`.

Exploit harness: `exploit_http.py` + `suite_exploits.toml` — see [security-http-exploits.md](security-http-exploits.md).
