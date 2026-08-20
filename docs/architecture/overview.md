# Architecture overview

Li is implemented as a **multi-stage compiler** in C++ that lowers programs to **LLVM IR**, then links a small C runtime. This page is the map; exact type rules live in the [language design spec](../superpowers/specs/2026-05-14-li-language-design.md).

Some boxes below are **planned** (Lean gate, deferred annotations). See **[Provability gaps](../verification/provability-gaps.md)** for what is wired today.

## End-to-end pipeline

```
.li source
    │
    ▼
┌─────────┐
│  Lexer  │  tokens + indentation
└────┬────┘
     ▼
┌─────────┐
│ Parser  │  AST
└────┬────┘
     ▼
┌──────────────┐
│ Name resolve │
└────┬─────────┘
     ▼
┌─────────────────────┐
│ Deferred annotations │  PEP 649-style lazy type resolve
└────┬────────────────┘
     ▼
┌──────────────────────────┐
│ Typecheck + borrow check │  Python 3.14 rules + li extensions
└────┬─────────────────────┘
     ▼
┌─────────┐
│   MIR   │  SSA-ish IR, simd ops, bounds checks
└────┬────┘
     ▼
┌─────────┐
│  LLVM   │  vector ISAs, -O0 dev / -O3 release
└────┬────┘
     ▼
  native binary + li_rt
```

## Compiler modules (planned)

| Module | Responsibility |
|--------|----------------|
| `lexer/` | Tokens, indentation, literals |
| `parser/` | AST, Pratt expressions |
| `ast/` | Node definitions + spans |
| `types/` | Python 3.14 checker, borrow, effects |
| `mir/` | Lower typed AST; SIMD, calls, control flow |
| `codegen/` | MIR → LLVM IR |
| `diagnostics/` | Source locations, hints |
| `lic/` | CLI: `check`, `build`, flags |

Build with **CMake + Ninja**; one static library per stage so incremental rebuilds stay fast.

## Runtime

`runtime/li_rt.c` provides:

- `li_panic`, `li_bounds_fail` (see [bounds-release-path](../verification/bounds-release-path.md))
- Helpers for `print` (Phase 4)
- No GC — owned heap types (`list`, `dict`) call `raises Alloc`

## Standard library

Shipped as `.li` under `std/` after Phase 4. Benchmarks and Tetris link against std + `extern` C (SDL2).

## Validation layers

| Layer | What |
|-------|------|
| `li-tests/` | Parse/type/prove/borrow/race/benchmark correctness — `run_all.sh` |
| `examples/tetris/` | End-to-end UI + game logic |
| `benchmarks/` | Physics correctness, then cross-lang perf |

## Compiler ↔ package isolation (carve-out)

The compiler is **upstream-only**: `compiler/`, `runtime/`, `bootstrap/`, and the `std/` facades never depend on, embed, or hardcode the downstream package tree (`packages/`). Packages are user code *compiled by* `lic`, never *linked into* it.

- `compiler/` + `runtime/` are the only CMake targets; `scripts/build.sh` builds nothing else.
- `std/` imports only `std.*`/`core`/`prelude` facades — never a package.
- Every package is top-level `packages/<name>/li.toml` (no nesting), and each `[dependencies]` entry is a sibling `path = "../<name>"` — no absolute or compiler-internal paths.
- A package like `li-aimd` is **installable standalone**: copying it plus its transitive dependency closure into a fresh tree must build and run with `lic` alone.

Enforced by CI gates:

| Gate | Enforces |
|------|----------|
| `scripts/check-compiler-isolation.sh` | No `packages/` refs in compiler/runtime/bootstrap source or CMake; no package imports from `std/`; packages top-level; deps are sibling paths only |
| `scripts/check-aimd-standalone.sh` | `li-aimd` + transitive deps build and run in a fresh tree outside the monorepo |

## Self-hosting (Phase 6)

The C++ compiler is a **bootstrap host**. Once the language and stdlib are rich enough, `lic` will be rewritten in Li and compiled by the C++ binary. Benchmarks guard against regressions during that transition.

## Related

- [Getting started](../getting-started.md)  
- [Master plan](../superpowers/plans/2026-05-14-li-master-plan.md)  
