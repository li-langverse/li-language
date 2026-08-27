# Physics docs: monorepo package paths and rigid composable smoke

## Summary

Agent-facing physics docs now reference `packages/li-*` folders and ergonomic imports (composable rigid test remains version smoke until struct-by-value move rules are clear).

## Agent continuation

1. Read `docs/physics/overview.md`, `GAME_DEV.md`, `SIMULATION_UI_READINESS.md`.
2. Run `./li-tests/run_all.sh composable` after `lic` build.
3. Next: publish org mirrors for `li-physics-*` per lic #50.
4. Blocked: none.

## Changed

- `docs/physics/overview.md`, `SIMULATION_UI_READINESS.md`, `GAME_DEV.md`, `numerical-policy.md`
- `docs/language/philosophy.md`, `fp-numerical-stability.md`
- `li-tests/composable/import_physics_runtime.li` — unchanged version smoke (integrate test blocked on move semantics)

## Not changed

- Package APIs or compiler import resolver.

## Breaking

N/A.

## Security / Performance / Downstream

N/A.
