# li-scene

Entity IDs, `Transform3`, scene graph hooks, and **MD particle display tiers** (1k / 10k / 100k @ 60/60/30 fps) with honest FPS reporting (`native_pixels=0` until wgpu draws).

Bench hook: `bench/particle_tiers.toml` (read by `./scripts/bench-studio-viewport-perf.sh`).

See [SIMULATION_UI_READINESS.md](../../docs/physics/SIMULATION_UI_READINESS.md).

```bash
lic check packages/li-scene/src/lib.li
```
