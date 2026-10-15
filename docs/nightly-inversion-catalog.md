# Nightly MIR parity: known accept/reject inversion class

Status as of the post-b424319c walker. The nightly sweep compares the
self-hosted Li MIR walker (`build/li-mir-walker`) against the C++ host
(`build/compiler/lic/lic`) over every parseable `.li` file. Byte-exact
MIR dump parity is the contract; the tables below pin the remaining
accept/reject boundary.

## Census over all `.li` files (mir subcommand, rc only)

| rc pair                    | files | meaning |
|----------------------------|------:|---------|
| both-accept, dump MATCH    |   358 | byte-exact parity (nightly MATCH class) |
| both-accept, dump DIFF     |     0 | no known dump differences remain |
| both-reject                |    17 | agreed rejects (also SKIP class) |
| C++ reject, walker accept  |    86 | inversion class (below) |
| walker reject, C++ accept  |     0 | none — walker rejects nothing C++ accepts |

The empty walker-reject/C++-accept cell is the important invariant: porter
schema changes must never reject what the C++ host accepts.

## Inversion class: 86 files, ~40 diagnostic classes

The C++ host rejects these during its analyze/typecheck/borrow-verify
stages, which the walker does not implement classes for. Largest classes
(normalized message -> files):

- 25 — EN: `X` is not allowed for value-returning procedures
-  6 — parallel_requires_disjoint: parallel for requires proved disjoint slices
-  4 — proc calls extern but does not declare raises IO
-  4 — array index out of range
-  3 — use after move of `X` (borrowck.cpp move semantics)
-  3 — expected `)` / 3 — EN: call violates requires clause
-  2 — unknown type `bytes` / proc missing ensures clause / stdlib symbol
  shadow / @vectorized lanes-only / unnamed `:` parser rejects
- singles: unknown type/proc/method, `Any` forbidden, generic return type
  mismatch, heap-type-without-raises-Alloc, duplicate definitions, mut
  borrow misuse, int-width mixing, bounded array index, import_cycle,
  false disjoint proof via verification, extern contract requirement,
  reserved stdlib names, postcondition-impossible, unreachable EmitN.

Full per-file catalog: `/tmp/inv_msgs.txt` (this is the session census
source; regenerate with the nightly sweep + rc census loop).

## Decision

Implementing the analyze surface inside the bootstrap walker is a
project-level port (borrowck, effect system, proof-carrying parallel
checks, policy walks). Each new reject must provably not flip any of the
358 byte-exact files, so classes should be ported one at a time with the
full 462-file census as the no-false-reject gate. Until then these files
remain baseline-SKIP: the nightly rc=0 state is exact, and MIR dump
parity across the both-accept class is complete.
