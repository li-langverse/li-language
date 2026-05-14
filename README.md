# Li

**理** — principle, reason. Source files: `.li`. Compiler: `lic`.

**Prove it. Write it easily. Run it fast.**

Li is a compiled language for HPC and scientific computing built on **three pillars** — in strict priority order:

1. **Mathematical provability** — Lean 4 kernel; mandatory contracts; no binary without proof  
2. **Easy syntax** — Nim-like surface, Python 3.14 types (no `Any`)  
3. **Fast execution** — LLVM, SIMD, multi-core OpenMP in v1  

If a feature cannot be proved, it does not ship. Speed and syntax never bypass the proof gate.

> **Status:** Phase 0 bootstrap — C++ skeleton + LLVM smoke test.

## The proof gate

```bash
lic build module.li   # types + memory + contracts + Lean → binary or REJECT
lic check module.li   # IDE only — not a certificate
```

Every `proc` carries `requires` / `ensures`; every loop carries `invariant` / `decreases`. No `Any`, `unsafe`, or `sorry`.

## Why Li

| Pillar | You get |
|--------|---------|
| **Provability** | Energy bounds, index safety, parallel disjointness — **theorems**, not hopes |
| **Syntax** | Indentation, `list[T]`, `dict[K,V]`, refinements `{i \| 0 ≤ i < N}` |
| **Speed** | Native code after proof; SIMD + `parallel for` in v1 |

## Quick links

| Document | Description |
|----------|-------------|
| [Documentation](https://cap-jmk-real.github.io/li-language/) | Published MkDocs site |
| [Docs source](docs/index.md) | Edit locally with `./scripts/build-docs.sh` |
| [Formal verification](docs/verification/overview.md) | Provable-only model |
| [Language design spec](docs/superpowers/specs/2026-05-14-li-language-design.md) | **Three pillars**, types, contracts |
| [Master plan](docs/superpowers/plans/2026-05-14-li-master-plan.md) | Implementation phases |
| [li-tests](li-tests/README.md) | **All tests** — manifest + `run_all.sh` |
| [X-ready plots](docs/superpowers/plans/2026-05-14-plots-and-social.md) | `./scripts/plot_shareables.sh` |

## License

MIT OR Apache-2.0

## Roadmap

1. C++ compiler + **Lean verification pipeline** (Phases 2e–2f)  
2. Types, contracts, MIR, LLVM + OpenMP  
3. Tetris (proved `game_step`) + **proved parallel** MD/N-body benchmarks  
4. Self-host  
