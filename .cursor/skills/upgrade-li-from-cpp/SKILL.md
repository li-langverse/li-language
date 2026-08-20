---
name: upgrade-li-from-cpp
description: >-
  Discipline for evolving the self-hosted Li compiler (bootstrap/lic/main.li).
  Heavy changes are prototyped and validated in the C++ host first, then ported
  to Li only after they pass tests and benchmark well. Use when improving the
  self-hosted lexer/parser/AST/typecheck/MIR/codegen, fixing a compiler bug
  surfaced while self-hosting, or adding a feature to bootstrap/lic/main.li.
---

# Upgrade Li compiler from C++

The C++ compiler (`compiler/`) is the **production and reference host**. The
self-hosted compiler (`bootstrap/lic/main.li`) is a *faithful port* of it,
built layer by layer. Both stay alive: C++ is the fast-iteration ground where
heavy changes are designed, benchmarked, and proven; Li receives only
*validated* logic, so the self-hosted source stays stable and high-quality.

## The rule in one line

> Find the improvement → implement it in **C++** → benchmark + test → if it
> benchmarks well and passes tests, **upgrade the Li compiler from there**.

Never the reverse: do not prototype a heavy change directly in Li.

## Why this order

- **C++ iterates fast**: full debug tooling, LLVM IR dumps, profilers, and the
  existing benchmark harness (`benchmarks/`, `scripts/plot_shareables.sh`).
- **Li is the long-term goal but slow to iterate**: it is compiled by the C++
  host and must stay byte-identical to the reference on the parity gates.
  Churning it with unvalidated ideas breaks parity and pollutes the port.
- **Parity is measured, not assumed.** The gates exist to prove the Li source
  is a faithful port; a change should only cross into Li once C++ has proven it.

## Workflow

### 1. Find the improvement

The trigger is usually one of:

- A **parity-gate divergence** (`check_li_lexer_parity.sh`,
  `check_li_parser_parity.sh`, `check_li_ast_parity.sh`,
  `check_li_self_frontend.sh`).
- A **compiler bug surfaced while self-hosting** — e.g. a MIR/codegen bug that
  only shows up when Li source exercises a pattern the C++ front end never
  generated (the array-store `Index`-index miscompile is the canonical case).
- A **missing feature** the next self-host layer needs to express itself.

State the gap precisely: input, expected vs actual behavior, and the layer it
belongs to (lexer / parser / AST / typecheck / MIR / codegen).

### 2. Implement in C++ first

Make the change in `compiler/` (lexer, parser, ast, types, mir, codegen,
verify) — the same code that ships in production `lic`. Keep the change as the
smallest vertical slice that closes the gap.

### 3. Validate in C++ (the gate)

Before any Li work:

- **Tests**: run the relevant `li-tests` suite(s)
  (`./li-tests/run_all.sh <suite>`) plus the full run if the change is broad.
- **Benchmark**: run the tier1_micro / tier2_physics benchmarks and compare to
  baseline. A change that fixes a bug must not regress; a change meant as an
  *optimization* must actually benchmark better.
- **Fail fast**: if tests fail or benchmarks regress, iterate in C++ only.
  **Do not touch `bootstrap/` yet.**

Only a change that passes tests **and** benchmarks well is eligible to move on.

### 4. Upgrade the Li compiler from there

Port the now-validated C++ logic into `bootstrap/lic/main.li`:

- Translate the algorithm faithfully — same semantics, same quirks, same
  token/emit behavior. Li has no top-level `var`; thread state (symbol tables,
  buffers) as parameters like the existing parser does.
- Rebuild with `./scripts/bootstrap_lic.sh` (C++ host compiles the Li source).
- Re-run the **parity gates** and confirm byte-identical output:
  - `scripts/check_li_lexer_parity.sh`
  - `scripts/check_li_parser_parity.sh`
  - `scripts/check_li_ast_parity.sh`
  - `scripts/check_li_self_frontend.sh`
  - Full sweeps via `LI_PARSER_FULL_SWEEP=1` / `LI_AST_FULL_SWEEP=1`.
- Keep the two in lockstep: a port is done only when the gates are green, not
  when it "looks right".

### 5. When a heavy port itself is risky

If translating the validated C++ change into Li is large or error-prone, split
the port into the same smallest vertical slices as the C++ work and gate each
slice with the parity scripts. Never accept a Li change that diverges the
reference.

## Hard rules (never violate)

| Priority | Rule |
|----------|------|
| 0 | **Keep the C++ compiler.** It is the production host and the reference for every port until full self-hosting is proven end-to-end. |
| 1 | **C++ first.** Heavy changes are prototyped and validated in `compiler/` before any `bootstrap/` edit. |
| 2 | **Two-gate bar.** A change reaches Li only after it passes tests *and* benchmarks well in C++. |
| 3 | **Byte parity.** Every port must re-pass the lexer/parser/AST/self-frontend parity gates (and full sweeps). |
| 4 | **No Li-only prototypes.** If a change can't be expressed or validated in C++, it is not ready for Li. |
| 5 | **One reference.** C++ is the source of truth; Li is a port. Never let the two accumulate independent semantics. |

## Self-host layer map

| Layer | What lives in `bootstrap/lic/main.li` | Status |
|-------|----------------------------------------|--------|
| 1 | CLI / argv bridge (Phase 6 seed) | done |
| 2 | `lex` — token-stream parity vs C++ lexer | done (492/492) |
| 3 | `parse` + `ast` — parser and int-encoded AST-dump parity vs `lic parse` / `lic ast`; plus the self-front-end gate (Li front end processes its own source) | done (482/482) |
| 4 | `check` — name resolution, type unification, contract well-formedness, borrowck/effects vs `lic check` | in progress |
| 5 | MIR lowering in Li | not started |
| 6 | LLVM codegen in Li (or stage-2: Li-compiled `lic` compiling itself) | not started |

The C++ host remains the compiler that builds `bootstrap/lic/main.li` and runs
the parity gates until Layer 6 lands and a Li-compiled `lic` can compile itself.

## Related skills and scripts

- `build-li-master-plan` — master plan phase executor (Phase 6 is the self-host
  seed; this skill is the discipline for growing it past the seed).
- `agent-diagnose-fix-li` — triage/fix workflow when a C++-side bug is found.
- `scripts/bootstrap_lic.sh` — build the Li binary from Li source.
- `scripts/check_li_{lexer,parser,ast,self_frontend}_parity.sh` — parity gates.
- `docs/superpowers/plans/2026-05-14-phase-06-self-host.md` — layer roadmap.
