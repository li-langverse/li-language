# Simulation, UI, and game-dev readiness

**Track:** PH-5b (physics) + PH-IO (ingest/UI) + `li-ui` / `li-scene`

## Shipped (monorepo workspace)

| Layer | Package (`packages/`) | Import | Role |
|-------|------------------------|--------|------|
| Math | `li-math` | `import math` | Vec2/3, Quat, Mat4, AABB |
| Numerics | `li-math-numerics` | `import math.numerics` | Euler, Verlet, three-body helpers |
| Physics core | `li-physics-core` | `import physics.core` | Tiers, profiles, `SimulationParams` |
| Physics domains | `li-physics-*` | `import physics.<area>` | Rigid, fluids, particles, EM, … |
| **Runtime** | `li-physics-runtime` | `import physics.runtime` | `physics_world_new`, `physics_step`, scene sync hooks |
| **Scene** | `li-scene` | `import scene` | `EntityId`, `Transform3`, `Scene` graph hooks |
| **UI** | `li-ui` | `import ui` | `Color`, `Rect`, `UiFrame`, `InputState`, `StudioShellLayout`, `PaintFrame` (layout + paint IR) |
| Compiler std | `std/io`, `std/ui`, … | `import std.io` / facades | Prelude stubs for PH-IO-4 / UI tooling |
| Benches | `benchmarks/tier2_physics/` | — | 20 kernels (catalog on benchmarks `main`) |

Workspace imports resolve via `import_name` in each package `li.toml` (see [import-style.md](../language/import-style.md)).

## Integration loop (engine)

```text
ui_frame_begin → input poll → physics_sync_from_scene → physics_step → physics_sync_to_scene → draw
```

Fixed timestep: `physics_world_game_default()` → `physics_step(world, 1.0/60.0)`.

## Next (priority)

1. **PH-IO-4** — wire `std/io` + `std/csv` in compiler (CSV ingest without Python).
2. **Import graph** — composable `import_physics_runtime.li` integrates `physics.rigid` (`var RigidBody`); extend with `physics.runtime` `physics_step` smoke; full composable object-field codegen tracked in release note `2026-05-19-rigid-var-param-composable`.
3. **Pure-Li tier-2** — expand `three_body_pure` and `horner_pure_li` (PH-7e).
4. **Render bridge** — `li-ui` → engine draw list; keep rendering out of physics (GAME_DEV.md).
5. **Publish mirrors** — org repos for physics/UI packages (see lic #50).

## Verification

```bash
cd lic
lic check packages/li-ui/src/lib.li
lic check packages/li-scene/src/lib.li
lic check packages/li-physics-runtime/src/lib.li
lic check std/io/io.li std/ui/ui.li
./scripts/check-li-def-syntax.sh packages
```

Tier-2: `cd benchmarks && python3 lic/benchmarks/harness/bench.py` (with sibling `lic`).
