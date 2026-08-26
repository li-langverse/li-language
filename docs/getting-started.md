# Getting started

This page orients you in the repository. For a friendlier walkthrough, start with the [Guide](guide/hello-world.md).

## What Li is

Li is a compiled language for scientific and systems programming where **only provable programs build**. You write readable code with small promises (`requires`, `ensures`, `decreases`); `lic build` checks them before producing a binary.

## Install

See [Getting started — tools](guide/getting-started-tools.md) for macOS, Linux, and Windows notes.

## First commands

```bash
./scripts/build.sh
./build/compiler/lic/lic build examples/hello.li -o hello
./hello
```

## Learn by example

| Goal | Page |
|------|------|
| Hello world | [guide/hello-world.md](guide/hello-world.md) |
| SIMD + parallel | [guide/fast-math-and-parallelism.md](guide/fast-math-and-parallelism.md) |
| More snippets | [guide/examples-gallery.md](guide/examples-gallery.md) |

## Go deeper

| Topic | Page |
|-------|------|
| All types and features | [language/overview.md](language/overview.md) |
| Compile pipeline | [compiler/build-pipeline.md](compiler/build-pipeline.md) |
| Mathematical provability | [compiler/why-provable.md](compiler/why-provable.md) |
| Tests & security | [testing/overview.md](testing/overview.md) |

## Repository layout

```
li/
  compiler/     # lic — lexer, types, MIR, LLVM
  runtime/      # Small C runtime (print, OpenMP driver, …)
  std/          # Standard library (.li)
  examples/     # hello, tetris, …
  benchmarks/   # Physics & perf harness
  li-tests/     # All automated tests
  docs/         # This documentation
```

## Implementation order

Follow the [master plan](superpowers/plans/2026-05-14-li-master-plan.md) — do not skip the Lean verification phases for release builds.
