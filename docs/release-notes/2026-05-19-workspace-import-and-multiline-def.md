# Workspace import fix + multiline `def` params + composable rigid smoke

## Summary

`import physics.rigid` now resolves to `packages/li-physics-rigid` (not `std/physics/rigid.li`) by parsing `members = [...]` correctly in `packages/li.toml`; the parser accepts multiline parameter lists; composable `import_physics_runtime.li` runs one semi-implicit integrate step.

## Agent continuation

1. Read `compiler/types/import_resolve.cpp` (`parse_workspace_members`) and `compiler/parser/parser.cpp` (`skip_param_layout`).
2. Run `./scripts/ci.sh` or `./li-tests/run_all.sh composable` after `lic` build.
3. Next: composable smoke for `import physics.runtime` / `physics_step`; sync package mirrors if APIs changed.
4. Blocked on: `li-physics-*` org mirror publish (lic #50).

## Changed

- `compiler/types/import_resolve.cpp` — `members = [` array parse (skip `[workspace]` section header)
- `compiler/parser/parser.cpp` — newlines/indents inside `def (...)` parameter lists; bare `return` for `-> unit`
- `packages/li-physics-rigid/src/lib.li`, `packages/li-physics-runtime/src/lib.li` — `b: var RigidBody` on `rigid_integrate_semi_implicit`
- `packages/li-physics-relativity/src/lib.li` — `var float` on `lorentz_gamma` / `relativistic_momentum` velocity param (borrowck)
- `packages/li-physics-runtime/src/lib.li` — `substep_inv` for float substep dt; `var PhysicsWorld` on step/substep procs (typecheck; full `lic build` of runtime lib still hits codegen crash — follow-up)
- `li-tests/composable/import_physics_runtime.li` — version + integrate smoke
- `li-tests/encapsulation/def_multiline_params.li`, `li-tests/manifest.toml`
- `docs/physics/SIMULATION_UI_READINESS.md`

## Not changed

- `std/physics/rigid.li` facade (still tag-only stub; workspace package wins when present).
- Import resolver order (workspace before std) — only member-list parsing fixed.
- Benchmarks dashboard thresholds; org mirror repos.

## Breaking

N/A — import resolution now matches documented workspace-first behavior.

## Security

N/A.

## Performance

N/A.

## Downstream

- Re-run `./scripts/sync-package-mirror-def-syntax-pr.sh` for physics-related mirrors after merge if package `src/lib.li` changed.
