# Changelog

## [Unreleased]

### Added

- **Vertical gap #4/#9 sim step physics** — `sim_scientific_tick_stub`, `studio_game_step_hook`, `studio_md_particle_tier_select_ok`, smokes `studio_sim_step_by_profile.li` / `import_studio_sim_step_by_profile.li` — [2026-05-25-vertical-gap-sim-step-physics.md](docs/release-notes/2026-05-25-vertical-gap-sim-step-physics.md).


### Fixed

- **Gap-close wave1 smokes** — `packages/li-studio/src/lib.li` passes `lic check` with all `li-tests/smoke/*.li` (timeline extern, move/copy fixes, MCP/viewport `raises IO`, inspector ensures); see `docs/release-notes/2026-05-25-studio-gap-close-wave1-smokes.md`.

### Added

- **PH-HW WP3 host present** — `studio_shell_input_from_host`, `studio_shell_host_frame`, `studio_shell_host_present_loop_tick`; `li_std_studio_version` → 6; smoke `studio_host_present.li`.
- **PH-SIM SIM-1** — `studio_sim_step_hook` (profile sync + `sim_step` tick stub).
- **PH-SIM SIM-0** — `studio_apply_profile_to_sim` (read-only `SimSessionStub`); `li-sim` path dependency; robotics profile id `4`.

### Changed

- **Breaking:** `studio_profile_sim_robotics()` id `5` → `4` (matches `li_rt_studio_profile_match_name`).

- **Viewport error recovery (UX-08)** — `studio_viewport_error_none`, `studio_err_gpu`, `studio_err_missing_asset`; `StudioViewportErrorOverlay` compose + stroke-only message/retry paint; `studio_viewport_error_retry()` mock via `li_rt_studio_viewport_error_*` (no wgpu failure probe); smoke `studio_viewport_error.li`.
- **Runnable shell demo entry (PH-GD-1)** — `src/main.li` (`studio_shell_demo_frame`, `li-studio-demo` bin), `examples/studio_shell_demo.toml`, smoke `studio_shell_demo.li`; headless compose/paint + keyboard hook per frame (no SDL/wgpu window).
- **Scene outliner (PH-GD-1)** — `StudioOutlinerNode`, `StudioOutlinerCompose`, `studio_compose_outliner`, `studio_paint_outliner`; demo hierarchy (Root, Camera, Mesh) in dock strip below slots; wired into `StudioShellCompose` and shell chrome counts; smoke `studio_outliner.li`.
- **Accessibility (UX-10)** — `studio_paint_focus_ring_for_panel` when `panel.active_region` matches; uses `li-ui` focus ring token.
- **Loading / skeleton (UX-11)** — `StudioShellLoadingState`, `studio_compose_shell_loading`, `studio_paint_shell_loading` (4 fill cmds); smoke `studio_shell_loading.li`.
- **Timeline playback (UX-02)** — `studio_timeline_playing`, `studio_timeline_toggle_play`, `studio_timeline_tick_frame` (+0.01 playhead/frame, cap 1.0); `play_btn_rect` + expanded timeline paint; smoke `studio_timeline_playback.li`.
- **Inspector fields (UX-03)** — `StudioInspectorField` + `studio_compose_inspector_selected` (≥2 label/value rows); empty path uses `studio_compose_empty_inspector`; smoke `studio_inspector_fields.li`.
- **Command palette (UX-04)** — `StudioCommandPaletteCompose` on `StudioShellCompose`; `studio_compose_shell_palette`, `studio_paint_palette_overlay`, `studio_handle_studio_key` / Cmd+K via `li-gui`; smoke `studio_command_palette.li` + `packages/li-ui` `studio_palette.li`.
- **MCP tool contracts (PH-AGENT / AGENT-0)** — `studio_mcp_*` tool IDs, `studio_mcp_tool_name` / `from_name`, `StudioAgentToolRequest`, `docs/game-dev/studio-mcp-tools.md`, smoke `studio_mcp_tools.li`.
- **Runtime profiles (UX-05 / PH-SIM scaffold)** — `studio_profile_*` constants, `studio_profile_from_name`, `studio_parse_toml_profile_line`, `StudioProjectConfig`, topbar profile chip paint, `fixtures/studio.toml`.
- **Empty states (UX-07)** — `studio_compose_empty_inspector`, `studio_compose_empty_viewport`, `studio_empty_state_for_region`; muted placeholder paint with honest inspector/viewport cmd counts; shell wires empty paths when `has_selection == 0` or `scene_entity_count == 0`.
- **Agent chrome gap-close (UX-06)** — `StudioAgentProgress`, `agent_context_label` + `context_rect`, failed `retry_hint_rect` (stroke), `studio_agent_last_action_reversible()` stub; paint cmd counts per state/context.
- **Agent chrome (UX-06)** — `StudioAgentChromeCompose` with task states (idle/running/blocked/failed/done), cancel rect, error strip; `studio_paint_agent` + `studio_compose_shell_agent`.
- **Panel compose** — `StudioDockCompose`, `StudioTimelineCompose`, `StudioInspectorCompose` from `li-ui` shell layout.
- **Paint decomposition** — `studio_paint_dock`, `studio_paint_timeline`, `studio_paint_inspector` (playhead + track + selection header).
- **Shell frame** — `studio_shell_frame` wires compose panels with topbar, viewport grid, and agent strip.
- **Panel switch** — `studio_panel_switch_inspector` / `studio_panel_switch_timeline` on `GuiPanelState`.
