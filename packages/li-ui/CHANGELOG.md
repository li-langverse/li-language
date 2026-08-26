# Changelog

All notable changes to this package will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Accessibility (UX-10)** — `studio_color_focus_ring`, `studio_paint_focus_ring`, `studio_contrast_ratio_ok()` stub (WCAG AA 4.5:1 target in README); smoke `studio_accessibility.li`.
- **Skeleton tokens (UX-11)** — `studio_color_skeleton_muted` / `highlight` for loading paint IR.
- **Command palette (UX-04)** — `StudioCommandPaletteCompose`, `studio_palette_open` / `close` / `toggle`, `paint_studio_palette`; smoke `studio_palette.li`.
- **Design tokens** — `studio_color_border`, `studio_color_accent_amber` (TOML sync for timeline playhead + borders).
- Studio shell **layout IR** (`studio_layout.li`): adaptive dock/topbar/viewport/inspector/timeline/agent-strip rects aligned to design tokens.
- `layout_panel_switch_within_budget_ms` — PH-UX gate helper (≤100 ms token).

- Studio **paint IR** (`studio_paint.li`): fill/stroke/grid ops + `paint_studio_shell_chrome` composable for li-gui bridge.
- `li-tests/composable/import_ui_studio_shell.li` — monorepo `compile_ok` for `import ui`.
- Initial scaffold via `scripts/li-new-package` (PKG-li-std-ui).

## [0.1.0] - 2026-05-16

### Added

- Package skeleton.
