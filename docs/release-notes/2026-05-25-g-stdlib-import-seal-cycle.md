# Release notes: 2026-05-25 — g-stdlib-import-seal-cycle

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PH / REQ:** Phase 4s, **G-stdlib**

## Summary (one sentence)

`check_stdlib_seal` runs on every module loaded by `resolve_imports`, with tests for import-graph shadowing and cyclic imports.

## Agent continuation (required)

1. Read: `docs/superpowers/specs/2026-05-16-li-stdlib-seal.md`, `docs/verification/provability-gaps.md` (**G-stdlib**).
2. Run: `LI_REPO_ROOT=$PWD cmake --build build --target lic && ./li-tests/run_all.sh stdlib_seal modules`.
3. Then: Phase **8a** re-export read-only sealed names if needed.
4. Blocked on: none.

## Changed (specific)

- `compiler/types/import_resolve.cpp` — seal in `load_module_recursive`
- `li-tests/stdlib_seal/shadow_echo_via_import.li`, `shadow_echo_dep.li`
- `li-tests/modules/import_cycle_a.li`, `import_cycle_b.li`
- `docs/verification/provability-gaps.md` — **G-stdlib** closed slice

## Not changed (scope fence)

- Workspace `import_name` vs std facade precedence
- Re-export override (**8a**)
- `std/**` coverage harness
- Lean / **G-lean**

## Breaking changes

None.

## Security

**G-stdlib:** blocks prelude shadow via dependency module (`stdlib_symbol_shadow`).

## Performance

N/A.

## Downstream

N/A.
