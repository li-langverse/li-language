# Release notes: physics game-dev readiness

**Date:** 2026-05-16  
**Repo:** lic  
**Branch:** `feat/physics-game-dev`

## Summary

Merges `feat/physics-module-packages` with Vision-LLM JSON fix (`type.index`), game-dev guide, and runtime defaults (60 Hz, 4 substeps).

## Added

- `docs/physics/GAME_DEV.md` — packages, benches, engine checklist
- `examples/game_physics_smoke/` — fixed-tick loop shape
- `li-tests/physics/game_runtime_smoke.li`

## Changed

- `compiler/diagnostics/diagnostics.cpp` — agent codes (`E0201` → `type.index`) in JSON output
- `li-std-physics-runtime` — `physics_world_game_default`, default `substeps = 4`

## Benchmarks

Tier-2 game benches present: `rigid_body_stack`, `ragdoll_chain`, `cloth_swing`, `euler_fluid_2d`, `sph_dam_break_2d`, …

## Paired

- Merge **#4** (`fix/typecheck-ctx-init`) before this branch to `main`
- **benchmarks** catalog ingest after lic merge
