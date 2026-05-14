# Formal verification in Li

**Mathematical provability is Li’s #1 pillar** — above easy syntax and above fast execution.

**Normative spec:** [The three pillars](../superpowers/specs/2026-05-14-li-language-design.md#the-three-pillars-priority-order)

## Priority order

1. **Prove** — Lean 4 kernel accepts all obligations, or **no `build`**  
2. **Write** — Nim-like syntax, Python 3.14 types, low boilerplate  
3. **Run** — LLVM, SIMD, `parallel for` — **only after** proof  

Tradeoff rule: **provability always wins.** Syntax sugar and `-O3` never skip Lean.

## The proof gate

| Command | Certificate? |
|---------|----------------|
| `lic build` | **Yes** — binary iff Lean kernel OK |
| `lic check` | **No** — fast IDE feedback only |

## Required on every compiling unit

- Types + refinements (no `Any`)  
- `requires` / `ensures` on every `proc`  
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

- [Architecture overview](../architecture/overview.md)  
- [Benchmarks plan](../superpowers/plans/2026-05-14-benchmarks-and-simulations.md)  
