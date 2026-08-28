# Composable imports prefer workspace packages over std facades

## Summary

Ergonomic imports like `physics.relativity` resolve to monorepo `packages/*` when listed in `packages/li.toml`, falling back to `std/` facades only when no workspace member matches.

## Agent continuation

1. Read `compiler/types/import_resolve.cpp` (`resolve_module_path`).
2. Run `./li-tests/run_all.sh composable` after building `lic`.
3. Next: align `docs/ecosystem/composable-by-default.md` examples with `import_name` from each package `li.toml`.
4. Blocked: none.

## Changed

- `compiler/types/import_resolve.cpp` — workspace lookup before `easy_std_module`.
- `li-tests/composable/import_{httpd_lib,physics_relativity,physics_runtime}.li` — canonical `import_name` paths.
- `li-tests/manifest.toml` — enable `import_physics_runtime` composable test.

## Not changed

- `def` syntax policy or package mirror repos.
- Std facade files under `std/physics/*` (still used when no workspace package).

## Breaking

N/A for external users; in-monorepo `import physics.*` may bind to package APIs instead of std tag-only facades.

## Security

N/A.

## Performance

N/A.

## Downstream

Package mirrors unchanged; behavior applies when building against full `lic` workspace.
