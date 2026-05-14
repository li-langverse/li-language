# Li

**理** — principle, reason. Source files: `.li`. Compiler: `lic`.

<div class="grid cards" markdown>

-   :material-shield-check:{ .lg .middle } **Prove it**

    ---

    Lean 4 kernel, mandatory contracts. No binary without proof.

-   :material-feather:{ .lg .middle } **Write it easily**

    ---

    Nim-like syntax, Python 3.14 types — without `Any`.

-   :material-lightning-bolt:{ .lg .middle } **Run it fast**

    ---

    LLVM 18, SIMD, OpenMP — only after the proof gate passes.

</div>

## The proof gate

```bash
lic build module.li   # types + memory + contracts + Lean → binary or REJECT
lic check module.li   # IDE only — not a certificate
```

Every `proc` carries `requires` / `ensures`; every loop carries `invariant` / `decreases`.
Forbidden: `Any`, `unsafe`, `sorry`, bare `cast`, unproved `parallel for`.

## Three pillars (strict priority)

| # | Pillar | Rule |
|---|--------|------|
| 1 | **Mathematical provability** | Never compromised |
| 2 | **Easy syntax** | Nim-like, Python 3.14 − `Any` |
| 3 | **Fast execution** | LLVM — only after proof |

## Quick start

```bash
export LLVM_DIR="$(brew --prefix llvm@18)/lib/cmake/llvm"   # macOS
export CC=clang CXX=clang++
./scripts/build.sh
./build/compiler/lic/lic --version
./scripts/local-ci.sh
```

See [Getting started](getting-started.md) for Linux prerequisites and repo layout.

## Where to go next

| Topic | Page |
|-------|------|
| Compile pipeline | [Architecture](architecture/overview.md) |
| Lean gate & contracts | [Verification](verification/overview.md) |
| Type system & numerics | [Language design spec](superpowers/specs/2026-05-14-li-language-design.md) |
| Implementation order | [Master plan](superpowers/plans/2026-05-14-li-master-plan.md) |
| Physics & perf harness | [Benchmarks](benchmarks.md) |
| All tests | [li-tests on GitHub](https://github.com/cap-jmk-real/li-language/tree/dev/li-tests) |

## Status

Phase 0–3 bootstrap: C++ `lic` parses, typechecks, and emits LLVM for a growing subset.
Self-host and full Lean pipeline are on the [roadmap](superpowers/plans/2026-05-14-li-master-plan.md).
