# Numerical policy and compile-time method selection (target)

## Goal

Users declare **physics intent** and **numerical targets**; the compiler chooses integrators, solvers, and approximations at **compile time** (no runtime registry).

## User surface (planned)

```li
@physics(tier = simulation, fps = 60)
def step_world(world: PhysicsWorld, dt: float) -> unit
  requires dt > 0
  targets energy_drift < 1e-3
  targets momentum_drift < 1e-4
```

## Defaults (`NumericalTargets`)

| Field | Default | T0 arcade | T2 scientific |
|-------|---------|-----------|---------------|
| `max_energy_drift` | 1e-3 | 5e-2 | 1e-4 |
| `max_momentum_drift` | 1e-4 | 1e-2 | 1e-6 |
| `target_fps` | 60 | 60 | N/A |
| `cfl_safety` | 0.9 | 0.8 | 0.95 |

## Selection rules (v1 table)

| Problem class | T0 | T1 | T2 |
|---------------|----|----|-----|
| ODE (generic) | Semi-implicit Euler | Velocity Verlet | RK4 / symplectic |
| N-body | Euler + softening | Verlet | Barnes–Hut + Verlet |
| Grid PDE | Explicit Euler | CFL-limited explicit | CN / implicit |
| Fluids | PBD / cheap SPH | WCSPH | MLS-MPM |
| Quantum | — | — | Split-operator TDSE |

## Agent skill

Contributors use **`.cursor/skills/research-li-numerics`** before adding a new kernel: survey references, document error bounds, add a Tier-0 stability row.

## Proof surface

- `requires dt > 0`, CFL inequalities as `requires` where explicit schemes are chosen
- `ensures` energy drift bounds for symplectic integrators when **G-vc** / **G-lean** support them

**Status:** Library types in `li-physics-core` today; decorator elaboration is **planned** (see [provability gaps](../verification/provability-gaps.md)).

For **floating-point expression stability** (subtraction cancellation, compensated sums), see [fp-numerical-stability.md](../language/fp-numerical-stability.md) (`lic build --numerically-stable`).
