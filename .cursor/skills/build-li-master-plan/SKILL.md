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

1. Read the **phase completion tracker** in the master plan; pick the **first unchecked** phase.
2. Read that phase's plan file (table in master plan). If marked **Stale**, rewrite it for **C++17 + CMake + Ninja + LLVM 18** before coding.
3. Read canonical context only as needed:
   - `docs/superpowers/specs/2026-05-14-li-language-design.md` (pillars, types, contracts)
   - `.cursor/rules/li-project.mdc`, `compiler-cpp.mdc`, `li-tests.mdc`
4. Announce: *Executing Phase N — &lt;name&gt;*.
5. Create todos for the phase tasks; execute until the phase **exit gate** passes or you hit a **hard blocker**.

**Do not ask "should I continue?"** after each subtask. Continue through the current phase exit gate. Only stop for blockers or when the phase gate is green.

## Hard rules (never violate)

| Priority | Rule |
|----------|------|
| 1 | **Provability** — no user-facing feature that bypasses Lean/contracts gate |
| 2 | **Stack** — C++ compiler only in `compiler/`; **LLVM 18** sole backend; no Rust/Zig host |
| 3 | **Tests** — new behavior → fixture in `li-tests/` + `manifest.toml` entry |
| 4 | **Truth** — update master plan checkboxes only after verification commands pass |

## Per-phase loop

```
READ phase plan
→ IMPLEMENT (smallest vertical slice first)
→ VERIFY (commands in phase plan exit gate)
→ REGISTER tests if applicable
→ COMMIT (one logical commit per task group)
→ CHECK master plan box
→ NEXT phase (same session if context allows)
```

### Verification (required before claiming done)

- Phase 0: `cmake --build build && ./build/compiler/lic/lic smoke-llvm`
- Later phases: phase plan exit gate + `./li-tests/run_all.sh <suite>`
- Use **verification-before-completion** skill before telling the user a phase is complete.

## Blockers — stop and report

Stop only when:

- LLVM 18 / CMake missing and cannot be installed in the environment
- Plan instruction is ambiguous on a design decision that affects pillars
- `li-tests` or phase gate fails after **two** distinct fix attempts

Report: what was done, exact error, what the user must provide (e.g. `brew install llvm@18 cmake`).

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
