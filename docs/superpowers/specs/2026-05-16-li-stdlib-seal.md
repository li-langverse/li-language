# Stdlib seal (prelude immutability)

**Phase:** 4s (security). **Status:** implemented in `lic check` / `lic build`.

## Goal

User modules cannot redefine or hijack names owned by the language prelude or shipped `std/` modules. This blocks decorator hijack, builtin substitution, and duplicate top-level confusion.

## Rules

1. **Prelude types** (`int`, `list`, `dict`, `tuple`, `Option`, `simd`, …) — no user `type` alias with the same unqualified name.
2. **Prelude procs** (`echo`) — no user `def` with the same name.
3. **Std module exports** — symbols defined in `std/**/*.li` (e.g. `__execution_decorators_doc`) cannot be redefined in user code.
4. **Reserved decorators** — `parallel`, `cpu`, … remain blocked for `decorator def` (see execution-decorators spec); shared table in `compiler/types/prelude.cpp`.
5. **Duplicates** — two top-level `def` or `type` with the same name in one file → `duplicate_definition`.

## Diagnostics

| Code | Meaning |
|------|---------|
| `stdlib_symbol_shadow: NAME` | User definition clashes with prelude or std export |
| `duplicate_definition: NAME` | Repeated top-level definition |
| `reserved_name: NAME` | User `decorator def` steals std decorator name |

## Implementation

- `li::check_stdlib_seal` in [compiler/types/prelude.cpp](../../../compiler/types/prelude.cpp), called from [compiler/lic/main.cpp](../../../compiler/lic/main.cpp) on the entry module and from [compiler/types/import_resolve.cpp](../../../compiler/types/import_resolve.cpp) for each resolved import.
- Manifest sync: [scripts/gen-stdlib-manifest.sh](../../../scripts/gen-stdlib-manifest.sh).

## Tests

`li-tests/stdlib_seal/` — wired in CI security and master-plan gates.

## Import graph (4s closed slice)

- `check_stdlib_seal` runs on the entry module and on every file loaded by `resolve_imports` (`load_module_recursive` in `import_resolve.cpp`).
- Cyclic imports fail with `import_cycle: <path>` (`li-tests/modules/import_cycle_a.li`).

## Future (8a)

Imported `std/*` bindings are read-only in the importer; re-export cannot override sealed names.
