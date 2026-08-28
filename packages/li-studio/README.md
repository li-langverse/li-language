# li-studio

Li World Studio product shell: composes **dock**, **timeline**, and **inspector** panels from `li-ui` layout IR and `li-gui` paint primitives.

Import: `import studio`

## Run demo (PH-GD-1)

**Headless compose/paint contract demo** — not an SDL/wgpu window yet. The runnable entry exercises `studio_compose_shell_palette` → `studio_paint_shell_chrome` and one `studio_handle_studio_key` call per frame; a native host must wire input and present paint IR later.

```bash
cd packages/li-studio
lic check src/main.li
lic build src/main.li -o li-studio-demo
./li-studio-demo   # exits 0 when 3-frame shell contract holds
```

Dimensions and frame budget: `examples/studio_shell_demo.toml` (1280×720, 3 frames). Smoke: `li-tests/smoke/studio_shell_demo.li`.

## Compose API

- `studio_compose_shell` / `studio_compose_shell_profile` — layout + `StudioProjectConfig.active_profile`
- `studio_profile_from_name` / `studio_parse_toml_profile_line` — PH-SIM profile stub (`fixtures/studio.toml`)
- `studio_paint_topbar_profile` — topbar chip; `last_rect.h` encodes active profile id
- `studio_compose_outliner` / `studio_paint_outliner` — scene hierarchy stub (Root, Camera, Mesh) in dock strip below slots (PH-GD-1)
- `studio_paint_compose_panels` — paint dock slots, outliner rows, timeline track/playhead, inspector chrome
- `studio_shell_frame` — full editor chrome (panels + topbar + viewport grid + agent chrome)
- **UX-08** — `studio_err_gpu` / `studio_err_missing_asset`, `StudioViewportErrorOverlay`, `studio_viewport_error_retry()` (mock; native wgpu probe not wired)
- `studio_compose_agent_chrome` / `studio_paint_agent` — task status, step progress, context label, cancel, error strip, retry hint (UX-06)
- `studio_panel_switch_inspector` / `studio_panel_switch_timeline` — PH-UX panel switch hooks
- `studio_compose_shell_loading` / `studio_paint_shell_loading` — UX-11 skeleton rects (viewport + inspector fields); no spinner paint IR
- `studio_paint_focus_ring_for_panel` — UX-10 focus stroke when `panel.active_region` matches (see `li-ui` `studio_paint_focus_ring`)

## Loading / skeleton (UX-11)

| Field / API | Purpose |
|-------------|---------|
| `StudioShellLoadingState` | `shell_loading`, `viewport_skeleton`, `inspector_*_skeleton` rects on compose |
| `studio_compose_shell_loading(..., shell_loading)` | Sets skeleton rects when `studio_shell_loading_on()` |
| `studio_paint_shell_loading` | Four muted `paint_op_fill_rect` cmds (honest placeholder, not a spinner) |
| `studio_shell_loading_frame` | Chrome + skeleton paint for smoke / host bootstrap |

## Accessibility (UX-10)

- `studio_paint_focus_ring_for_panel` paints one stroke around `studio_region_rect_for_focus` when the active region is not the default viewport.
- Contrast: `studio_contrast_ratio_ok()` lives in `li-ui` (stub `1.0`; WCAG AA **4.5:1** target documented there).
- **Follow-up:** axe in CI when `world-studio-demo` ux-harness exists — not wired in package smoke yet.

## Agent chrome (UX-06)

| Field / API | Purpose |
|-------------|---------|
| `StudioAgentProgress` | `step_index`, `step_total`, `progress_rect`; `visible == 1` only when `task_state == running` (determinate bar, not a spinner) |
| `agent_context_label` | Context id on compose; painted as `context_rect` stroke inside status |
| `studio_agent_context_world()` | Label id `1` → display **world.li** |
| `studio_agent_context_selection()` | Label id `2` → display **selection: Node** |
| `retry_hint_rect` | Failed-state retry affordance (stroke only) |
| `studio_agent_last_action_reversible()` | Undo contract stub; returns `0` until host wires undo |

Failures use `studio_color_agent_error()` on status + error strip; running state never masks failed tasks.
