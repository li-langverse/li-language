# Li physics module overview

Physics for the Li game engine ships as **`li-physics-*`** workspace packages on **`li-math`** and **`li-math-numerics`**, with ergonomic imports (`import physics.runtime`, `import math`).

**Quick start:** [GAME_DEV.md](./GAME_DEV.md) (packages, tier-2 benches, engine checklist).

> **Org mirrors:** some published repos still use legacy names (`li-std-math`, `li-httpd`). Monorepo folders and `import_name` values use `li-math`, `li-physics-*`, etc. See [repo-naming.md](../ecosystem/repo-naming.md).

## Fidelity tiers

| Tier | Name | Typical use |
|------|------|-------------|
| T0 | Arcade | 60+ FPS, PBD, semi-implicit Euler |
| T1 | Simulation | Verlet, SPH, weather advection |
| T2 | Scientific | FDTD, TDSE, weak-field GR |
| T3 | Research | Offline HEP toys, full MD research |

Scenes select a **`PhysicsProfile`** (`tier`, `dt`, `substeps`, **`NumericalTargets`**).

## Packages (monorepo `packages/`)

| Folder | Import | Role |
|--------|--------|------|
| `li-math` | `import math` | Vec3, Quat, Mat4, array dot/sum |
| `li-math-numerics` | `import math.numerics` | Integrators, three-body reference |
| `li-physics-core` | `import physics.core` | Profiles, tier defaults |
| `li-physics-rigid` | `import physics.rigid` | Rigid bodies, PGS stub |
| `li-physics-runtime` | `import physics.runtime` | `PhysicsWorld`, `physics_step` |
| `li-physics-particles` | `import physics.particles` | Emitters, MD SoA |
| `li-physics-fluids` | `import physics.fluids` | SPH, heat, cloth PBD |
| `li-physics-weather` | `import physics.weather` | Advection/diffusion wind |
| `li-physics-aero` | `import physics.aero` | Atmosphere, orbits |
| `li-physics-chem` | `import physics.chem` | Reactions, passive combustion |
| `li-physics-em` | `import physics.em` | Coulomb, FDTD, Poisson |
| `li-physics-quantum` | `import physics.quantum` | TDSE 1D hooks |
| `li-physics-relativity` | `import physics.relativity` | SR + Schwarzschild factor |
| `li-physics-hep` | `import physics.hep` | Toy MC (education) |

## Benchmarks

Tier-2 kernels live under `lic/benchmarks/tier2_physics/`. Status is tracked on the [benchmarks dashboard](https://li-langverse.github.io/benchmarks/).

## Numerical policy

See [numerical-policy.md](numerical-policy.md) for compile-time method selection (planned language phase **PHY-n**).
