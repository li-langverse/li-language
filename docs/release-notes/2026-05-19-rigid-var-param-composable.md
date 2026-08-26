# Composable physics: document integrate gap (version smoke only)

## Summary

Records why `li-tests/composable/import_physics_runtime.li` stays a version smoke (`import physics.rigid`) until composable builds accept imported object types and non-`Ident` integrate calls.

## Agent continuation

1. Read `li-tests/composable/import_physics_runtime.li` and `compiler/types/borrowck.cpp` (`check_call_moves`).
2. Run `./li-tests/run_all.sh composable` after `lic` build — must stay green with version-only fixture.
3. Next: fix composable compile for `RigidBody` locals + `rigid_integrate_semi_implicit(body, …)` or add `b: var RigidBody` end-to-end; see ecosystem-gap issue from this PR.
4. Blocked on: publishing `li-physics-*` org mirrors (lic #50).

## Changed

- `docs/physics/SIMULATION_UI_READINESS.md` — composable status (integrate deferred)
- `docs/release-notes/2026-05-19-rigid-var-param-composable.md` (this file)

## Not changed

- `packages/li-physics-rigid` / `li-physics-runtime` APIs (no `var RigidBody` param in this PR)
- Import resolver, `std/` facades, mirror repos

## Breaking

N/A

## Security / Performance / Downstream

N/A

## Evidence

CI on branch `cursor/rigid-var-param-57b4`: composable tests that reference imported `RigidBody` or call `physics_world_game_default()` fail `compile_ok`; version-only `li_std_physics_*_version()` passes.
