# li-gui

Native Li Studio GUI layer: viewport region extraction, panel-switch timing hooks, and paint-IR expansion over `li-ui` shell composables.

## Viewport (UX-01)

- `ViewportSelection` — optional marquee (`active=0` none; `active=1` + `rect` + `depth_cue`).
- `gui_viewport_region_from_layout` — no selection; `gui_viewport_region_from_layout_with_selection` — exposes `selection_active` on `ViewportRegion`.

## Keyboard (UX-09)

| Input | Action | `gui_handle_studio_key` result |
|-------|--------|----------------------------------|
| `Escape` | Close command palette when open | `studio_key_action_palette_close` (2) |
| `Cmd/Ctrl+K` | Toggle command palette | `studio_key_action_palette_toggle` (1) |
| `1` | Focus dock | `studio_key_action_region_focus` (3) |
| `2` | Focus viewport | region focus (3) |
| `3` | Focus inspector | region focus (3) |
| `4` | Focus timeline | region focus (3) |
| `5` | Focus agent strip | region focus (3) |

- `InputState.key_cmd_k`, `key_digit` — set by host/SDL ingest; defaults in `input_default()`.
- `gui_handle_studio_key(panel, input)` — panel-only palette flag.
- `gui_handle_studio_key_palette` / `studio_handle_studio_key(compose, input)` — full `StudioCommandPaletteCompose` via `studio_palette_*` in `li-ui`.

Import: `import gui`
