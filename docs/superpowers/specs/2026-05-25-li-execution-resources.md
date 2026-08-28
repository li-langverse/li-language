# Execution resource model (normative)

**Date:** 2026-05-25  
**Companion:** [Native execution surface](2026-05-25-li-execution-surface.md)  
**Plan:** `.cursor/plans/compiler_lint_and_parallelism_330a4d8e.plan.md` — Phase G

## Four axes (do not conflate)

| Axis | User surface | CLI / config | Must **not** use |
| ---- | ------------ | ------------ | ---------------- |
| **SIMD / vector lanes** | `@vectorized(lanes=N)`, `simd[T,N]` | reject `--vector-lanes` | `--cores`, `--jobs` |
| **Runtime parallelism** | `parallel for`, `@parallel(disjoint=…)` | `--cores`, `--threads-per-core`; `[execution]` | `--jobs` |
| **Compile / check farm** | not in `.li` source | `--jobs`, `--max-memory` | `--cores`, `@vectorized` |
| **RAM budget** | optional embed | `--memory-budget-mb` | thread team size |

## Config (`li.toml`)

```toml
[execution]
cores = 8
threads_per_core = 1
memory_mb = 4096
```

Flags win over manifest; env vars deprecated.

## Agent quick-check

```text
MD across cores     → parallel for + --cores
dot in one thread   → @vectorized(lanes=8)
fast li-tests       → run_all.sh -j + --max-memory
```

See [execution surface](2026-05-25-li-execution-surface.md).
