# Phase 2a: Type Core (C++)

**Exit gate:** `LIC=./build/compiler/lic/lic ./li-tests/run_all.sh typecheck prove_reject`

**Delivered on `dev`:**
- `lic check` / `lic build` (parse + typecheck; codegen stub)
- int/float/array alias checking, literal bounds
- Forbidden `Any`, mandatory `requires`/`ensures`
- Source policy: `parallel for` + `par_slice` → disjoint error

**Pending:** full Python 3.14 scalar/union parity (mypy fixtures)
