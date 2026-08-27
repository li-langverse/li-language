# Li physics for game development

**Status:** tier-2 benchmarks + `li-physics-*` workspace packages on `lic` `main`  
**Oracle:** shared C kernels in `benchmarks/tier2_physics/*/common/*_core.c` (Li matches cpp/rust/julia shape)

---

## Packages (import style)

Use **short modules** — see [import-style.md](../language/import-style.md):

```li
import physics.runtime
import physics.rigid
import scene
import ui
import math
```

| Import | Use in games |
|--------|----------------|
| `physics.core` | Units, constants, shared types |
| `physics.rigid` | Bodies, AABB/sphere overlap, PGS normal stub |
| `physics.particles` | Particles, simple forces |
| `physics.fluids` | Grid/fluid helpers (tier-2 euler/sph benches) |
| `physics.runtime` | `PhysicsWorld`, `physics_step`, scene sync hooks |
| `scene` | `EntityId`, `Transform3`, `Scene` graph |
| `ui` | `Color`, `Rect`, `UiFrame`, `InputState` |

**Entry API:** `physics_world_new(2)` → loop `physics_step(world, 1.0/60.0)` → `physics_sync_to_scene`.

---

## Tier-2 benches (game-adjacent)

| Bench id | Game use |
|----------|----------|
| `nbody_gravity` | Many-body / orbital particles |
| `double_pendulum` | Chaotic constraints |
| `rigid_body_stack` | Stacked colliders |
| `ragdoll_chain` | Articulated character chain |
| `cloth_swing` | Cloth / rope grid |
| `sph_dam_break_2d` | Fluid splash |
| `euler_fluid_2d` | Grid smoke/water (Euler) |
| `three_body_pure` | Pure-Li integrator path (PH-7e) |

Run harness:

```bash
cd lic/benchmarks/harness
python3 bench.py --list
python3 bench.py rigid_body_stack ragdoll_chain cloth_swing euler_fluid_2d
```

Catalog + dashboard: `li-langverse/benchmarks` `catalog.toml` → ingest.

---

## Integration checklist (engine)

1. **Fixed timestep** — `physics_world_new`; set `substeps` ≥ 1 for stability.  
2. **Scene sync** — implement `physics_sync_from_scene` / `physics_sync_to_scene` bodies when binding to entities.  
3. **Prove game step** — game logic `def game_step(...) ensures …` calls `physics_step` under contract (see `examples/tetris/` pattern).  
4. **Validate** — tier-0 + tier-2 green; render GIFs via benchmarks `render-benchmark-visuals.sh`.  
5. **Numerics** — skill `research-li-numerics`; study doc for novel integrators.  
6. **Scalar precision** — per-sim `PhysicsProfile.float_bits` / `ScalarPrecision`; see [Scalar precision](../language/scalar-precision.md) (`float32`, `binary` weights — not org-enforced).

---

## PR / merge order (org)

1. **lic** `fix/typecheck-ctx-init` / #4 — compiler blocker (merge first).  
2. **Package CI** — `li-net`, `li-httpd`, `li-std-core`, `li-std-math`, `li-demo` (org mirrors).  
3. **lic** `main` — `li-physics-*`, `def` syntax, composable imports (#58–#64).  
4. **benchmarks** — catalog/ingest PRs after lic `main` builds.

Run org sweep: `python3 scripts/run-pr-program.py` in **benchmarks** repo.

---

## World Studio (vision)

Long-term **AI-first editor** (local models, proved worlds, agent APIs) — not a Unity clone on day one:

→ **[World Studio & Li Engine vision](../game-dev/world-studio-vision.md)** (PH-GD + PH-SIM/ROBO/AM/SCI/DRUG/QM/UX/AGENT).  
→ **[Program tracker](../game-dev/PH-world-studio-program.md)** · **[RFCs](../game-dev/specs/README.md)**

## Out of scope (tier R)

Rendering (shadows, BRDF, particles on GPU) — physics-only in PH-5b until **PH-GD-5** (`li-render`). Use your engine renderer + Li physics state until then.
