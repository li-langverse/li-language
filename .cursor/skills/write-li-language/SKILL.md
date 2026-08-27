---
name: write-li-language
description: >-
  Write and evolve Li programs after the self-host bootstrap is established:
  syntax, contracts, refinement-safe arrays, effects, borrowing, package
  boundaries, and C++-first compiler parity discipline.
---

# Write Li language

Li is proof-first and layout-sensitive. Treat the C++ compiler as the
reference implementation until a Li-compiled `lic` can compile itself. For a
compiler change, follow `upgrade-li-from-cpp`: implement and benchmark the
behavior in C++, pass the relevant tests, then port the validated behavior to
`bootstrap/lic/main.li` and run parity.

## Source shape

- Use `def` for ordinary procedures and `extern def` only for a deliberately
  small runtime/OS seam.
- Keep the signature explicit: parameter types, return type, and `raises`
  effects belong in the declaration.
- Every procedure has `requires`, `ensures`, and `decreases` clauses. Use
  `ensures true` only when there is no useful postcondition; do not hide a
  proof obligation behind an open VC.
- Bodies use Li indentation and a standalone `=` body marker. Do not use
  legacy `proc` syntax in new code.
- Prefer direct expressions and named helpers over clever parser/compiler
  tricks; the C++ host and the Li port must retain the same semantics.
- Write diagnostics for users and agents, not just implementers: state what
  the construct means, show the nearest valid Li example, and link the
  relevant `docs/language/*.md` page. Never conflate absence (`None`) with
  `unit` or an empty collection; use an explicit `Option[T]` model.

## Contracts and refinement-safe arrays

- State input assumptions in `requires`; state the returned value and mutated
  state in `ensures`.
- For loops, provide a terminating `decreases` expression and an invariant
  when the loop establishes a useful bound or conservation law.
- Array indices must carry a visible proof/refinement. A common pattern is a
  bounded index type such as `type PosN = {p: int | 0 <= p and p < N}` and a
  helper that accepts `PosN` before indexing `array[N, T]`.
- Keep float constants typed consistently; avoid mixing integer and float
  arithmetic without an explicit conversion or a matching overload.
- For closed fixed-size vectors/matrices, prefer `@`, `dot`, `sum`, and the
  typed array operators so MIR lowering can preserve shape facts.

## Effects and ownership

- Declare `raises IO`, `Alloc`, `Net`, or `Async` when calling a function with
  that effect. Keep pure math and proof helpers effect-free.
- Treat `var` parameters as in/out borrows. Mutations must be intentional and
  must propagate through the caller ABI; do not copy a mutable object merely to
  silence borrow errors.
- Expect E0310 for overlapping mutable borrows and E0311 for use-after-move.
  Fix ownership at the call boundary instead of weakening the checker.
- Keep package imports composable and standalone. The compiler/runtime must
  not import downstream packages.

## Compiler-port workflow

1. Reproduce a bug or missing feature in a focused `li-tests` fixture.
2. Implement it in `compiler/` first, run the relevant suite and benchmark.
3. Port only the validated algorithm to `bootstrap/lic/main.li`.
4. Rebuild the stage-2 binary with `scripts/bootstrap_lic.sh` or the parity
   script.
5. Run `scripts/check_li_parity.sh`; for MIR work also use
   `LI_MIR_FULL_SWEEP=1 scripts/check_li_mir_parity.sh` when disk/time permit.
6. Add the fixture to the appropriate manifest/corpus and record any known
   open layer honestly.

## Self-host boundary

The current bootstrap proves lexer, parser/AST, typecheck slices, and a
substantial MIR slice. It does **not** yet make `lic-from-li build` compile Li
source. Do not claim full self-hosting until a Li-compiled stage-2 binary can
compile `bootstrap/lic/main.li` (or an equivalent compiler source) without
invoking the C++ host, and the resulting stage-3 binary passes the same gates.
