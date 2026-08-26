# Native execution surface (syntax + ergonomics)

**Date:** 2026-05-25  
**Depends on:** [Execution resource model](2026-05-25-li-execution-resources.md), [Execution decorators](2026-05-16-li-execution-decorators.md)  
**Plan:** WP8 in compiler_lint_and_parallelism plan

## Design goal

Concurrency, parallelism, threads, cores, and SIMD feel **simple, flexible, powerful, and native** — one mental model: source → `li.toml` → `lic build` → binary.

## Principles

| User says | Li surface | Proof |
| --------- | ---------- | ----- |
| use 8 cores | `parallel for` + `--cores=8` or `[execution] cores = 8` | disjoint required (G-par) |
| vectorize inner | `@vectorized(lanes=8)` on inner `for` | no extra OS threads |
| CI fast check | `lic check --workspace --jobs=8` | not in `.li` source |

## Layer 1 — Keywords

`parallel for i in 0..<N` is primary (MIR `ParallelFor` → `li_parallel_for_*`). Requires `disjoint_*` or inherited `disjoint=` — else compile error.

## Layer 2 — Decorators (stackable, top-to-bottom)

`@parallel(disjoint=...)`, `@vectorized(lanes=N)` on `def` / `for` / `parallel for`. **SIMD** = one thread, N lanes. **Parallel** = cores × threads_per_core. **Compile farm** = `--jobs` only — never in source.

## Layer 3 — Config and CLI

```toml
[execution]
cores = 8
threads_per_core = 1
memory_mb = 4096
```

```bash
lic build app.li -o app --cores=8 --threads-per-core=1
lic check --workspace --jobs=8 --max-memory=4096
```

Deprecated: `--threads` alias; `LI_*` env (one-release warn).

## Phase 2 — `team(cores=4) { ... }`

Spec only until parser lands.

## Learned from

| System | Li adaptation |
| ------ | ------------- |
| Nim `parallel for` | keyword + proved disjoint |
| Rust rayon | `@parallel` inherits; static chunks v1 |
| numba | prange vs SIMD = separate knobs |
| Chapel | `team()` phase 2 |

Handbook: [parallelism.md](../../language/parallelism.md).
