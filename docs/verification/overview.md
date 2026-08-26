# Formal verification in Li

**Mathematical provability is Li’s #1 pillar** — above easy syntax and above fast execution.

**Normative spec:** [The three pillars](../superpowers/specs/2026-05-14-li-language-design.md#the-three-pillars-priority-order)

## Priority order

1. **Prove** — Lean 4 kernel accepts all obligations, or **no `build`**  
2. **Write** — Nim-like syntax, Python 3.14 types, low boilerplate  
3. **Run** — LLVM, SIMD, `parallel for` — **only after** proof  

Tradeoff rule: **provability always wins.** Syntax sugar and `-O3` never skip Lean.

## No user runtime (reliability)

Li is **compiled ahead of time**, not interpreted. The product goal is to drive **user logic errors to compile/proof time** so release binaries after **`lic build`** do not depend on catching bugs dynamically (bounds, races, shape errors, bad decorators).

The only unproved surface is **`trusted.lean`** (minimal `IO`). Decorators, math notation, and parallelism are **designed** to lower statically — see master plan § *Compile-time reliability*.

!!! important "Current gaps"
    **`lic build` does not yet run Lean 4** or full VC discharge. Parallel disjointness uses **heuristic** policy checks; decorators are **parse-only**. See **[Provability gaps (today)](provability-gaps.md)** and the **[proof database](proof-database/README.md)** before claiming a full proof certificate.

## The proof gate

| Command | Certificate? |
|---------|----------------|
| `lic build` | **Target:** binary iff Lean kernel OK · **Today:** parse + policy + typecheck + borrow + codegen (see [gaps](provability-gaps.md)) |
| `li-tests prove_lean_ok` | **Partial:** strict build + zero open AutoVC + `lake build AutoVC` when elan installed — see [proof corpus](proof-corpus-roadmap.md) |
| `lic check` | **No** — fast IDE feedback only |

## Required on every compiling unit

- Types + refinements (no `Any`)  
- `requires` / `ensures` on every `def`  
- `invariant` + `decreases` on every loop  
- Borrow/memory safety  
- Parallel: proved disjointness  
- **Zero** `sorry` / bare `cast` in user code  

## Trusted base (only exception)

`docs/semantics/trusted.lean` — minimal `IO` axioms. User logic never lives here.

## Lean 4 = Coq-grade bar

Proofs are **kernel-checked**, not test-passed, not SMT-only.

## Benchmarks

Tier 2 physics must **`lic build`** with full proofs (energy/momentum `ensures`) before perf comparisons.

## Honesty

- Contracts define the theorem; wrong spec → wrong proof.  
- Keep `trusted.lean` tiny.  
- Meta-proof of the C++ compiler is future work.

## Related

- [Proof database](proof-database.md)
- [Architecture overview](../architecture/overview.md)  
- [Benchmarks plan](../superpowers/plans/2026-05-14-benchmarks-and-simulations.md)  
