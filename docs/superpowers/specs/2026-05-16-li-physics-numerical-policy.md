# Physics numerical policy (language surface)

> **Status:** Planned phase **PHY-n** — library types ship in `li-std-physics-core`; compiler elaboration not wired.

## User-facing goals

1. Declare **fidelity tier**, **timestep budget**, and **error targets** once per simulation or `def`.
2. Compiler selects integrator, stencil, and solver **at compile time** from a closed table.
3. Reject unstable combinations (`dt` vs CFL, non-symplectic + long energy bound).

## Surface (draft)

```li
@physics(tier = simulation, fps = 60)
def advance(world: PhysicsWorld, dt: float) -> unit
  targets energy_drift < 1e-3
  targets momentum_drift < 1e-4
  requires dt > 0
```

## Elaboration

| Input | Pass | Output MIR |
|-------|------|------------|
| `tier = arcade` | `dt`, targets | Semi-implicit Euler, loose drift |
| `tier = simulation` | CFL for hyperbolic if grid | Verlet / WCSPH |
| `tier = scientific` | `dt` halving proof optional | RK4 / CN / FDTD |

## Depends on

- **2i / 7e** — math lowering
- **7d** — `@physics` decorator slot (or reserved name alongside `@cpu`)
- **2e / 2f** — `targets` as proof obligations where provable

## Tests

- `li-tests/physics_policy/` — reject illegal tier+target pairs
- Extend `benchmarks/harness/stability.py` per method class

## Agent workflow

Use skill **`research-li-numerics`** before extending the selection table.
