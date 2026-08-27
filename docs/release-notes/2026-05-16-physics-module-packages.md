# Physics module packages and numerics foundation

**Date:** 2026-05-16  
**Repo:** lic

## Summary

Initial `li-std-physics*` package family, expanded `li-std-math`, runtime transcendentals, Tier-2 benchmark kernels, and numerical-policy spec.

## Packages

- `li-std-math` — spatial types and ops
- `li-std-numerics` — integrators, three-body reference
- `li-std-physics-core` — `PhysicsProfile`, `NumericalTargets`
- `li-std-physics-rigid`, `-runtime`, `-particles`, `-fluids`, `-weather`, `-aero`, `-chem`, `-em`, `-quantum`, `-relativity`, `-hep`

## Compiler / runtime

- `li_rt_sin`, `li_rt_cos`, `li_rt_atan2`, `li_rt_exp`, `li_rt_log`

## Docs / agents

- `docs/physics/overview.md`, `numerical-policy.md`
- `docs/superpowers/specs/2026-05-16-li-physics-numerical-policy.md`
- Skill: `.cursor/skills/research-li-numerics`

## Benchmarks

New under `benchmarks/tier2_physics/`: `rigid_body_stack`, `three_body_pure`, `wind_field_bc`, `combustion_passive`, `orbit_two_body`, `fdtd_waveguide_2d`, `schrodinger_1d_barrier`

## Verification

- `lic build` on packages when LLVM 22 toolchain available
- `./benchmarks/harness/bench.py --tier 2 --ci` after compiler build
