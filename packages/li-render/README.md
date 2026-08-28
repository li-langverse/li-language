# li-render

Li Studio viewport render layer: **wgpu smoke** bridge (`li-gpu`), **FPS counter** hooks, and **scene MD particle draw** path (`li-scene` tiers 1k/10k/100k) for the bench harness (PH-UX viewport ≥ 60 fps; particles 10k@60 / 100k@30).

## FPS counter

- `RenderFpsCounter` + `render_fps_counter_tick` — rolling FPS estimate for HUD/bench.
- `render_fps_counter_hud_text` / `render_fps_counter_hud_tag` — UX-13 HUD contract (`native=0` until wgpu draws).
- `render_fps_counter_shell_tick` / `render_fps_counter_shell_simulate` — shell hook: 120 simulated frames @ ~16.667 ms (~60 Hz).
- `render_bench_fps_counter_simulate()` — alias of shell simulate for bench JSON (`meets_target=1` when math holds).
- Bench hook: `bench/viewport_fps.toml`.

## Viewport smoke

- `render_wgpu_viewport_smoke(ViewportRegion)` — ties `li-gui` viewport geometry to `gpu_wgpu_smoke_run()`.
- `native_pixels=0` until wgpu-rs surface records pixels (honest stub).
