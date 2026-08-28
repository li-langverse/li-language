# li-std-ui

2D UI types (`Color`, `Rect`, `InputState`) and frame hooks for Li games and simulation tools.

See [GAME_DEV.md](../../docs/physics/GAME_DEV.md) and [SIMULATION_UI_READINESS.md](../../docs/physics/SIMULATION_UI_READINESS.md).

```bash
lic build src/lib.li -o li-std-ui
```

## Accessibility (UX-10)

- `studio_color_focus_ring()` — `#3dd6ff` per `docs/design/studio-design-tokens.toml` `focus_ring`
- `studio_paint_focus_ring(frame, region_rect)` — one stroke-rect paint command for keyboard focus
- `studio_contrast_ratio_ok()` — stub returns `1.0` until host samples foreground/background; **WCAG AA target ratio 4.5:1** for normal text (documented contract, not enforced in IR yet)
- **Follow-up:** axe / native a11y scan in CI when `world-studio-demo` harness exists — do not add axe to package smoke until then
