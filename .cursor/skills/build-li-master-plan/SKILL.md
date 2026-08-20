---
name: build-li-master-plan
description: >-
  Execute the Li compiler master plan phase-by-phase without repeated user
  prompting. Loads phase plans, implements C++/LLVM compiler work, runs
  li-tests, updates the phase tracker, and advances to the next gate. Use when
  the user says continue the master plan, keep building Li, finish the next
  phase, or resume compiler implementation.
---

# Build Li — master plan executor

Autonomous workflow for implementing Li per `docs/superpowers/plans/2026-05-14-li-master-plan.md`.

## On every invocation

0. Read [strict-by-default.md](../../../docs/ecosystem/strict-by-default.md) and [engineering-standards.md](../../../docs/ecosystem/engineering-standards.md) (functionality, security, performance gates — always on).
1. Read the **phase completion tracker** in the master plan; pick the **first unchecked** phase.
2. Read that phase's plan file (table in master plan). If marked **Stale**, rewrite it for **C++17 + CMake + Ninja + LLVM 22** before coding.
3. Read canonical context only as needed:
   - `docs/superpowers/specs/2026-05-14-li-language-design.md` (pillars, types, contracts)
   - `.cursor/rules/li-project.mdc`, `compiler-cpp.mdc`, `li-tests.mdc`
4. Announce: *Executing Phase N — &lt;name&gt;*.
5. Create todos for the phase tasks; execute until the phase **exit gate** passes or you hit a **hard blocker**.

**Do not ask "should I continue?"** after each subtask. Continue through the current phase exit gate. Only stop for blockers or when the phase gate is green.

## Hard rules (never violate)

| Priority | Rule |
|----------|------|
| 0 | **Strict by default** — no optional provability; explicit `li.toml` `[gates]` or documented env only to relax |
| 1 | **Provability** — no user-facing feature that bypasses Lean/contracts gate |
| 2 | **Stack** — C++ compiler only in `compiler/`; **LLVM 22** sole backend; no Rust/Zig host |
| 3 | **Tests** — new behavior → fixture in `li-tests/` + `manifest.toml` entry |
| 4 | **Truth** — update master plan checkboxes only after verification commands pass |
| 5 | **Provability honesty** — if the PR touches proof surface (Lean, VC, parallel, decorators, math, bounds), update `docs/verification/provability-gaps.md` (**G-*** rows) and linked handbook pages in the **same PR** (master plan § Doc) |
| 6 | **`std/`** — **100%** line coverage before phase checkbox; run `scripts/check-stdlib-coverage.sh` when instrumented |
| 7 | **Packages** — new workspace members export composable `src/lib.li` (serve/stop/ready or equivalent); see `composable-li-library` skill |

## Per-phase loop

```
READ phase plan
→ IMPLEMENT (smallest vertical slice first)
→ VERIFY (commands in phase plan exit gate)
→ REGISTER tests if applicable
→ UPDATE provability-gaps.md (+ handbook) when proof surface changed (see master plan § Doc)
→ COMMIT (one logical commit per task group)
→ CHECK master plan box
→ PUSH `./scripts/agent-push-github.sh "feat(phase-N): …"` (see `.cursor/rules/li-auto-push.mdc` — never ask user to push)
→ NEXT phase (same session if context allows)
```

### Verification (required before claiming done)

- Phase 0: `cmake --build build && ./build/compiler/lic/lic smoke-llvm`
- Later phases: phase plan exit gate + `./li-tests/run_all.sh <suite>`
- Use **verification-before-completion** skill before telling the user a phase is complete.

## Blockers — stop and report

Stop only when:

- LLVM 22 / CMake missing and cannot be installed in the environment
- Plan instruction is ambiguous on a design decision that affects pillars
- `li-tests` or phase gate fails after **two** distinct fix attempts

Report: what was done, exact error, what the user must provide (e.g. `brew install llvm@22 cmake`).

## Phase → plan file map

| Phase | Plan |
|-------|------|
| 0 | `docs/superpowers/plans/2026-05-14-phase-00-bootstrap.md` |
| 1 | `docs/superpowers/plans/2026-05-14-phase-01-lexer-parser.md` |
| 2d | `docs/superpowers/plans/2026-05-14-phase-02-typechecker.md` (+ 2a–2c when added) |
| 3 | `docs/superpowers/plans/2026-05-14-phase-03-mir-codegen.md` |
| 4 | `docs/superpowers/plans/2026-05-14-phase-04-runtime-stdlib.md` |
| 5 | `docs/superpowers/plans/2026-05-14-phase-05-tetris.md` |
| 5b | `docs/superpowers/plans/2026-05-14-benchmarks-and-simulations.md` |
| 6 | `docs/superpowers/plans/2026-05-14-phase-06-self-host.md` |

- Phase 5b: `./scripts/plot_shareables.sh` for X-ready PNGs (`docs/superpowers/plans/2026-05-14-plots-and-social.md`)
- Phase 6 self-host: use **upgrade-li-from-cpp** — prototype/benchmark heavy changes in the C++ host first, then port to `bootstrap/lic/main.li` once they pass tests and benchmark well

## Commits

- `feat(phase-N): <what>` for functionality
- `chore: <what>` for scaffolding-only
- Init repo with `git init` on first commit if needed

## User interaction model

| User says | Agent does |
|-----------|------------|
| "keep going" / "continue master plan" | Resume from first unchecked phase |
| "start phase N" | Jump to that phase (still respect dependencies) |
| Nothing (skill auto-triggered) | Same as keep going |

No crates.io or Rust ecosystem checks — Li is not Rust.

## Additional resources

- Subagent parallelism: use **dispatching-parallel-agents** when phase tasks are independent (e.g. lexer + diagnostics stubs).
- Long sessions: use **executing-plans** discipline (todos, verify each step).
- After a phase gate: optionally use **finishing-a-development-branch** before merge/PR.
- Phase 6 self-host changes: use **upgrade-li-from-cpp** (C++-first → port-to-Li discipline) and `scripts/check_li_parity.sh` for the full front-end parity gate in one command.
