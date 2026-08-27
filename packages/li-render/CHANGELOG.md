# Changelog

## [Unreleased]

### Added

- **lig.present wire-up (PH-HW HW-1)** — `render_present_viewport_tick`, `render_fps_counter_host_tick`, `render_viewport_host_fps_counter`; `li_std_render_version` → 3.
- **UX-13 HUD** — `render_fps_counter_hud_text`, `render_fps_counter_hud_tag`, particle-tier HUD helper; `native=0` in sim strings.
- **Shell FPS hook** — `render_fps_counter_shell_tick` / `render_fps_counter_shell_simulate` (120× ~16.667 ms frames).
- **Scene MD particles** — `RenderMdParticleViewport`, `render_scene_md_particle_viewport` over `li-scene` tiers.
- **FPS counter** — `RenderFpsCounter`, tick/simulate helpers, `render_fps_counter_meets_target` bench hook.
- **Viewport smoke** — `RenderViewportSmoke`, `render_wgpu_viewport_smoke` bridging `li-gpu` + `li-gui`.
- **Bench hook** — `bench/viewport_fps.toml` for `bench-studio-viewport-perf.sh`.
