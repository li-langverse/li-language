# Getting started

This page helps you orient in the Li repository. Phase 0 bootstrap is live — build with CMake + LLVM 18 below.

## What you are looking at

Li is a **compiled** language for scientific computing where **only provable programs build**. Mandatory contracts + Lean 4; trusted IO only in `docs/semantics/trusted.lean`.

- **Syntax:** Nim-like (`proc`, indentation, `type` / `object` / `enum`)
- **Types:** Static, aligned with **Python 3.14** `typing`
- **Backend:** **LLVM 18** only
- **Compiler (v0):** **C++17** — self-host in Li comes later (Phase 6)

## Prerequisites (for when bootstrap exists)

You will need:

- LLVM **18** (`llvm-config` on `PATH`)
- **CMake** 3.20+ and **Ninja**
- A C++17 compiler (Clang recommended)
- Python 3.10+ (benchmark harness only)

On macOS, LLVM 18 is typically installed with Homebrew:

```bash
brew install llvm@18 cmake ninja
export LLVM_DIR="$(brew --prefix llvm@18)/lib/cmake/llvm"
export CC=clang CXX=clang++   # Xcode libc++; do not use brew's clang++ for lic
```

## Repository layout

```
li/
  compiler/       # C++ lic (lexer → parser → types → mir → codegen)
  runtime/        # li_rt.c — panic, bounds traps
  std/            # Standard library (.li) — grows after Phase 4
  examples/       # tetris/, hello/
  benchmarks/     # physics + ML + cross-lang harness (Phase 5b)
  li-tests/      # all tests — see li-tests/README.md
  docs/           # you are here
```

## How work is sequenced

Do not jump ahead of the [master plan](superpowers/plans/2026-05-14-li-master-plan.md):

| Phase | Deliverable |
|-------|-------------|
| 0 | C++ workspace, LLVM emits `main` |
| 1–2 | Parse + Python 3.14 typecheck |
| 3–4 | `lic build`, stdlib |
| 5 | Tetris |
| 5b | N-body, MD, ML benchmarks |
| 6 | Self-hosted compiler |

## First commands

```bash
export LLVM_DIR="$(brew --prefix llvm@18)/lib/cmake/llvm"
./scripts/build.sh
./build/compiler/lic/lic smoke-llvm
```

After Phase 5:

```bash
./build/compiler/lic/lic build examples/tetris/main.li -o tetris --release
./tetris
```

## Where to ask questions in the docs

| Question | Read |
|----------|------|
| What types exist? | [Design spec — type catalog](superpowers/specs/2026-05-14-li-language-design.md) |
| What integers and SIMD? | [Numeric roadmap](superpowers/specs/2026-05-14-li-language-design.md#numeric-roadmap) |
| What collections? | [Data structures roadmap](superpowers/specs/2026-05-14-li-language-design.md#data-structures-roadmap) |
| How do we benchmark? | [Benchmarks plan](superpowers/plans/2026-05-14-benchmarks-and-simulations.md) |

## Contributing documentation

See [Documentation style](contributing/documentation.md) before adding or rewriting docs.

Preview the site locally:

```bash
./scripts/build-docs.sh --strict
python3 -m http.server -d site 8000
```

Published at [cap-jmk-real.github.io/li-language](https://cap-jmk-real.github.io/li-language/) on merge to `main`.
