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

## MIR parity & type classification

The li `mir` subcommand (walker, compiled from `bootstrap/lic/main.li`) is the **reference** for the C++ `lic mir` dump. `scripts/check_li_mir_parity.sh` enforces byte-exact dumps on a 37-file gate; `LI_MIR_FULL_SWEEP=1` classifies every corpus file as match / known-diff / real-gap / cpp-only / li-only / neither.

Single owners to keep in lockstep (all drive a bit emitted by both sides):

- **Walker type-class predicates** — `compiler/mir/include/li/mir_types.hpp` holds every `is_*_type_name` helper (rd classes 1/2/4, pi2 is_i64, pointer-width unions). `compiler/mir/lower.cpp` and `mir_dump.cpp` must never re-derive a type-name list by hand; add the predicate there.
- **Runtime-link flags** (`needs_rt_net` / httpd / log bits in the MIR header) — `scan_runtime_flags` in `compiler/mir/lower.cpp`, which walks *every* expression-bearing statement position (the walker token-scans all `ident(`).
- **Dump-vs-codegen presentation remaps** — `compiler/mir/mir_dump.cpp` maps INS 13→11 (the walker emits 1D float loads as op 11; the internal enum keeps `ArrayLoadFloat` so codegen stays float-aware). Add a dump remap only if it actually flips a file.

`compiler/mir/mir_abi.cpp`, `mir_runtime_link.cpp`, and `num_stable.cpp` are **parked, not compiled** on mainline too (only `mir.cpp lower.cpp mir_dump.cpp` build into `li_mir`). Do not delete them to "clean up" — that would fork the tree from main; they are legacy from the E0360/num-stable/httpd feature lines.

## Related

- [Getting started](../getting-started.md)  
- [Master plan](../superpowers/plans/2026-05-14-li-master-plan.md)  
