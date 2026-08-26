# Bounds release path (G-bnd)

**Status:** **Partial** — see [provability-gaps](provability-gaps.md#g-bnd).

## Target

Release builds of programs with **proved** array indices must not rely on `li_bounds_fail` in `runtime/li_rt.c`. Dynamic indices without a refinement proof remain a compile-time error (E0201) or future debug-only traps.

## Pipeline today

| Stage | Proved index behavior |
|-------|------------------------|
| **Typecheck** | `refined_index_params` accepts `IndexN`-typed proc params; literals must be in range; other `int` indices rejected unless loop-bound |
| **VC emit** | Call-site refinement obligations in `AutoVC.lean` (`P-refine folded:` / `_proved`) |
| **MIR lower** | `ArrayLoadInt` / `ArrayStoreInt` set `index_is_literal` for `IntLit`, else `index_ident` for refinement-typed vars |
| **LLVM emit** | `CreateInBoundsGEP` only; `li_bounds_fail` is declared in the module but **not** called for current specimens |

## Evidence

| Artifact | Command |
|----------|---------|
| Refinement corpus | `li-tests/contracts_verify/bounds_refinement_release_ok.li` |
| Lean discharge | `li-tests/tooling/discharge_refinement_lean.sh` |
| Release IR | `LI_KEEP_LL=1` + `li-tests/tooling/check_release_bounds_ir.sh` |

## Still open (P-bnd)

- Emit `li_bounds_fail` on **unproved** dynamic indices in debug builds only (Phase 3 plan checkbox).
- Loop-carried refinement indices through full MIR/simd paths.
- Link-time proof that release objects never reference `li_bounds_fail` across the whole program (not just single-file specimens).
