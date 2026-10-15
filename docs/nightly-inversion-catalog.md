# Nightly MIR parity: known accept/reject inversion class

Status after two checker-class ports (post-b424319c walker: index bound,
extern-call effects). The nightly sweep compares the self-hosted Li MIR
walker (`build/li-mir-walker`) against the C++ host (`build/compiler/lic/lic`)
over every parseable `.li` file. Byte-exact MIR dump parity is the contract;
the tables below pin the remaining accept/reject boundary.

## Census over all `.li` files (mir subcommand, rc only)

| rc pair                    | files | meaning |
|----------------------------|------:|---------|
| both-accept, dump MATCH    |   358 | byte-exact parity (nightly MATCH class) |
| both-accept, dump DIFF     |     0 | no known dump differences remain |
| both-reject                |    24 | agreed rejects (also SKIP class) |
| C++ reject, walker accept  |    79 | inversion class (below) |
| walker reject, C++ accept  |     0 | none — walker rejects nothing C++ accepts |

The empty walker-reject/C++-accept cell is the important invariant: porter
schema changes must never reject what the C++ host accepts. It re-verified at
0 false rejects across a full rc census after each port.

## Checker-class port log

### 2026-10-15: extern-call raises-IO rule (second class)

C++ rule (compiler/types/borrowck.cpp `proc_mentions_extern_call` +
`effects_check_module`): a non-extern main-module proc is rejected with
`proc calls extern but does not declare raises IO` when a top-level body
statement's root expression is a direct call to a declared extern proc (or a
user-declared extern `print`; `echo` is excluded by name) and the proc does
not list `IO` in `raises`. Root expression means C++ `Stmt::expr`: bare Expr
statements, `return` exprs, and Assign RHS — not var-decl/borrow inits
(these land in `Stmt::init`), not conditions, not nested call arguments, not
nested blocks.

Walker mirror (bootstrap/lic/main.li `fx_mir_effects` + `fx_mir_top_stmt`,
run in mir mode before `mir_module`): a two-pass scan reuses the existing fx
collection machinery (proc names, extern flags, `raises` bits via
`fx_eff_bit`), then flags only the three root shapes at top level and
requiring the IO bit. Non-root call positions consume via the walker's own
quiet parser machinery so token flow stays identical.

- Flipped to both-reject: `volatile_sink_no_io.li`,
  `golden_dot4_ones_twos.li`, `golden_tier1_dot8.li`, `leak_censor.li`.
- Residue (known divergence, not implementation): calls to externs declared
  in OTHER files (imports) are outside the single-file scan because C++
  merges imported procs into its proc_map; no current census file relies on
  it.
- Deliberately out of scope (other C++ checker classes): the print/echo IO
  rule (`io_fail.li`), heap `raises Alloc`, await/async walker-only rules.

### 2026-10-15: static index bound check (first class)

C++ rule (compiler/types/typecheck.cpp): for every `Index` expression on an
array-typed base, an int-literal index must satisfy `0 <= i < array_size`;
otherwise the host rejects with `array index out of range`.

Walker mirror (bootstrap/lic/main.li `mir_lower_expr`, three literal-index
sites): generic fixed-array load, object-field load `o.x[i]`, nested
object-field load `d.tier.vals[i]`. Sizes come from the walker's canonical
tables only — current-proc local/param int/float array tables and object-field
element counts. Unknown sizes (named-type aliases, computed/temp bases) stay
unchecked, so the walker rejects a strict subset of what the C++ host does.

- Flipped to both-reject: `cwe120_stack_buffer_oob.li` (frame[40] on 32),
  `cwe125_negative_index.li` (buf[-1], folded negative literal),
  `board_oob.li` (board[200] on 200).
- Still open: `cwe787_oob_index.li` (`g: Grid` alias; alias RHS
  `array[8, T]` is not resolved to a size by `mir_type_info`, which reports
  named types as class 6 with no element count).
- Store-side literal indices (`arr[i] = v`) are not checked yet; no current
  census file reaches C++ through a store site alone.

## Remaining inversion class: 79 files

The C++ host rejects these during its analyze/typecheck/borrow-verify stages,
which the walker does not implement classes for. Remaining classes
(normalized message -> files):

- 25 — EN: `X` is not allowed for value-returning procedures
-  6 — parallel_requires_disjoint: parallel for requires proved disjoint slices
-  4 — proc calls echo or print but does not declare raises IO (the walker's
  mir-mode fx port deliberately does not set the print/IO bit yet)
-  1 — array index out of range (cwe787 alias residue, above)
-  3 — use after move of `X` (borrowck.cpp move semantics)
-  3 — expected `)` / 3 — EN: call violates requires clause
-  2 — unknown type `bytes` / proc missing ensures clause / stdlib symbol
  shadow / @vectorized lanes-only / unnamed `:` parser rejects
- singles: unknown type/proc/method, `Any` forbidden, generic return type
  mismatch, heap-type-without-raises-Alloc, duplicate definitions, mut
  borrow misuse, int-width mixing, bounded array index, import_cycle,
  false disjoint proof via verification, extern contract requirement,
  reserved stdlib names, postcondition-impossible, unreachable EmitN.

## Decision

Implementing the analyze surface inside the bootstrap walker is a
project-level port (borrowck, effect system, proof-carrying parallel
checks, policy walks). Each new reject must provably not flip any of the 358
byte-exact files, so classes are ported one at a time with the full
462-file census as the no-false-reject gate — exactly what the first two
ports satisfied. Until the remaining classes land, those files stay
baseline-SKIP: the nightly rc=0 state is exact, and MIR dump parity across
the both-accept class is complete.

Note: the nightly script requires bash >= 4 (`declare -A`); on macOS run it
with `/opt/homebrew/bin/bash` (or equivalent), the default /bin/bash is 3.2.
