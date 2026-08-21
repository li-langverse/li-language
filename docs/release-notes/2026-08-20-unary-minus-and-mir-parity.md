# Unary-minus codegen bug, extern int ABI mismatch, and self-hosted MIR parity

**Date:** 2026-08-20

## Summary

The self-hosted MIR walker in `bootstrap/lic/main.li` now emits **byte-exact**
`lic mir` parity over its scalar lowering corpus, and the parity diffing that
got it there surfaced four real compiler bugs — two in the C++ host (fixed
C++-first, then ported), two in the Li walker — plus a third Li walker bug.
Each bug now has a regression test or a parity gate so it cannot silently
recur.

## Compiler fix 1: unary minus on general expressions was silently dropped

The C++ parser folded `-1` / `-1.5` into negative literals, but for `-x` on a
general expression it **discarded the minus entirely** — `return -g()` compiled
to `return g()`, producing wrong code with no diagnostic.

Fix: a real `UnaryMinus` AST kind, added end-to-end in the C++ host first
(parser → AST dump node 72 → typecheck → borrowck → VC/Lean emitters → MIR
lowering as `0 - x` int / `0.0 - x` float), then ported to the Li bootstrap
(AST node 72 + MIR descriptor kind 11 + `li_rt_mir_f64_neg` for `-1.5`).

## Compiler fix 2: negative extern int args were zero-extended

`li_rt_ast_int` / `li_rt_mir_label` in the runtime took `int64_t` params while
the Li `int` ABI is `i32`, so `-1` reached them as `4294967295`. Changed the
signatures to `int32_t`.

## Self-hosted MIR-walker parity bugs (found by diffing `lic mir`)

1. **Float-name table leaked across procs** — a float param named `x` in one
   proc made a later int param `x` in another proc lower as float. The
   per-proc name tables are now reset at the start of each `mir_proc`.
2. **Emit pass could not resolve the first proc / recursion** — the emit pass
   used the emitted-proc count as the lookup count, so the first proc's body
   (and any recursive call) could not find itself in the proc table. `pn`
   stays at the total count during emit; a separate counter assigns pids.
3. **Literal-index array loads put the destination in the wrong cell** — the
   loaded value went into `ident` instead of `lhs_ident`, diverging from the
   C++ `ArrayLoadInt` field layout.

## Regression harness extensions

- **`li-tests/runtime/unary_minus.li`** (`verify_ok`) — `-g()` semantics,
  negative values through extern args and array round-trips.
- **`li-tests/codegen/neg_float_procs.li`** (`compile_open_ok`) — same param
  name in a float and an int proc must not leak float-ness; covers `-x`
  (int + float) and `-1.5`.
- **`scripts/check_li_mir_parity.sh` flipped from SKIP to a real gate** —
  byte-exact `lic mir` vs li-`mir` diff over the scalar corpus (fib recursion,
  unary minus, float/int negation, externs, if/for, arrays), wired into
  `scripts/check_li_parity.sh` and therefore `scripts/ci.sh`.

## Verification

```bash
./scripts/check_li_parity.sh        # lexer + parser + AST + self-front-end + check + MIR
./li-tests/run_all.sh runtime codegen
```

All six parity gates pass and the `runtime` / `codegen` suites are green.
`LI_MIR_FULL_SWEEP=1` remains the growing-coverage flag for the
array-specialized (`@` dot/matmul, elementwise) and object lowering that is
the next porting milestone.
