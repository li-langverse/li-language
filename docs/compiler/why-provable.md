# Why Li is mathematically provable

“Mathematically provable” means: the important properties of your program are **theorems** checked by a small, trusted proof engine — not hopes backed by testing alone.

## The proof gate

```
lic build  →  binary exists  ⟺  proofs closed   (target)
lic check  →  fast feedback only (no certificate)
```

**Today:** `lic build` runs parse, policy, typecheck, borrow, and codegen — **without** Lean yet. See **[Provability gaps](../verification/provability-gaps.md)**.

When Phase **2f** lands: if Lean still has open goals, **no executable ships**.

## What is being proved?

| Claim | Mechanism |
|-------|-----------|
| Types line up | Typechecker |
| Indices in range | Refinements + VCs |
| Memory safe | Borrow checker |
| Preconditions hold | `requires` / `ensures` |
| Loops terminate | `decreases` |
| Parallel loops race-free | Disjointness + Sync laws |
| No escape hatches | Reject `Any`, `sorry`, bare `cast` |

Together, these are stronger than “we fuzzed it.”

## Why Lean 4?

Lean’s **kernel** checks proof terms. If the kernel accepts a proof, the logical chain is valid relative to the axioms you started from.

Li is not “SMT said maybe” or “tests passed” — it is **proof objects** checked by the kernel.

This is the same assurance culture as **Coq**-style verification, using Lean 4 as the engine.

## What you write vs what Lean sees

You write Nim-like code with contracts. The compiler:

1. Typechecks and borrows.
2. Generates **verification conditions** (VCs).
3. Sends obligations to Lean.
4. Only then emits LLVM.

So the binary is a **compiled proof artifact**, not a separate “verified mode.”

## Trusted axioms (minimal)

Real programs need a little I/O. Li keeps a **small** trusted file:

`docs/semantics/trusted.lean`

Only this file may contain unproved axioms (bounded, reviewed). Application logic stays in user code with full contracts.

## Honest limits

| Limit | Explanation |
|-------|-------------|
| **Implementation gaps** | Features listed in the spec but not fully proved yet — **[gap register](../verification/provability-gaps.md)** |
| Wrong spec | You can prove the wrong theorem perfectly |
| Trusted base growth | Must stay tiny and audited |
| Compiler correctness | Proving the C++ compiler matches Lean is future meta-work |
| CPU behavior | Proofs are about the Li model, not flaky hardware |

## Parallelism and proof

Shared-memory parallelism is the hardest part of HPC correctness. Li’s answer: **reject** `parallel for` unless disjointness is stated and checked. See `li-tests/race_shared_memory/`.

## Learn more

- [Provability gaps (today)](../verification/provability-gaps.md)
- [Contracts and proofs](../language/contracts-and-proofs.md)
- [Verification overview](../verification/overview.md)
- [Language design — pillars](../superpowers/specs/2026-05-14-li-language-design.md)
