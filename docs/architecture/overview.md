# Architecture overview

Li is implemented as a **multi-stage compiler** in C++ that lowers programs to **LLVM IR**, then links a small C runtime. This page is the map; exact type rules live in the [language design spec](../superpowers/specs/2026-05-14-li-language-design.md).

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

- `li_panic`, `li_bounds_fail`
- Helpers for `echo` / printing (Phase 4)
- No GC — owned heap types (`list`, `dict`) call `raises Alloc`

## Standard library

Shipped as `.li` under `std/` after Phase 4. Benchmarks and Tetris link against std + `extern` C (SDL2).

## Validation layers

| Layer | What |
|-------|------|
| `li-tests/` | Parse/type/prove/borrow/race/benchmark correctness — `run_all.sh` |
| `examples/tetris/` | End-to-end UI + game logic |
| `benchmarks/` | Physics correctness, then cross-lang perf |

## Self-hosting (Phase 6)

The C++ compiler is a **bootstrap host**. Once the language and stdlib are rich enough, `lic` will be rewritten in Li and compiled by the C++ binary. Benchmarks guard against regressions during that transition.

## Related

- [Getting started](../getting-started.md)  
- [Master plan](../superpowers/plans/2026-05-14-li-master-plan.md)  
