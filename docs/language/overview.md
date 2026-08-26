# Language handbook — overview

Li is a **compiled**, **statically typed** language for science and systems that must be **correct**. This handbook describes the language as designed and notes what the current `lic` compiler accepts today.

For the normative technical spec, see the [language design spec](../superpowers/specs/2026-05-14-li-language-design.md).

## Design goals (plain language)

1. **No silent lies** — types and contracts must agree; the build fails otherwise.
2. **Readable code** — **does what it reads like it does** (Python-style simplicity: clear names, obvious flow). See [Philosophy](philosophy.md).
3. **Real speed** — after proof, LLVM produces native code with SIMD and multiple cores.

## Program shape

```nim
# optional types and imports at top level

def name(arg: T) -> R
  requires <precondition>
  ensures <postcondition>
  decreases <measure>
=
  <statements>
```

- **Top level:** `proc`, `type`, `object`, `enum`, `extern proc`.
- **No `Any`**, no `unsafe`, no `sorry` in user code.

## Handbook map

| Topic | Page |
|-------|------|
| Philosophy & naming | [Philosophy](philosophy.md) |
| Naming conventions (PascalCase types) | [Naming conventions](naming-conventions.md) |
| Full OOP roadmap (methods, traits) | [OOP roadmap](../superpowers/plans/2026-05-20-li-oop-roadmap.md) |
| Imports | [Import style](import-style.md) |
| Types & data | [Types and data](types-and-data.md) |
| Scalar precision (`float32`, `binary`, suffixes) | [Scalar precision](scalar-precision.md) |
| Math/physics at any width | [Precision polymorphism](precision-polymorphism.md) |
| Numbers | [Numerics](numerics.md) |
| Vectors & parallel | [SIMD and parallel](simd-parallel.md) |
| Contracts & proof | [Contracts and proofs](contracts-and-proofs.md) |
| Control flow & functions | [Control flow and functions](control-flow-and-functions.md) |
| Collections & generics | [Collections and generics](collections-generics.md) |
| Effects & I/O | [Effects and I/O](effects-and-io.md) |

## What every compiling program includes

| Feature | Required? |
|---------|-----------|
| `requires` / `ensures` on each `def` | Yes |
| `decreases` on each loop | Yes |
| `invariant` on `while` loops (when used) | Yes |
| Disjoint proof on `parallel for` | Yes |
| Explicit effects (`raises IO`, etc.) when using I/O | Yes |

## Commands

| Command | Purpose |
|---------|---------|
| `lic parse` | Syntax only |
| `lic check` | Fast feedback (not a certificate) |
| `lic build` | Full gate → binary |

## Status honesty

The compiler is **growing**. Some spec features are fully implemented; others are parsed or typechecked only. When in doubt, look at `li-tests/` for a working `.li` example or run `lic build` on your file.

**Canonical gap list:** [Provability gaps (current compiler)](../verification/provability-gaps.md) — what is **not** proved or not wired yet (Lean gate, decorator elaboration, math surface, heuristic parallel checks, …).

Implementation phases: [Master plan](../superpowers/plans/2026-05-14-li-master-plan.md).
