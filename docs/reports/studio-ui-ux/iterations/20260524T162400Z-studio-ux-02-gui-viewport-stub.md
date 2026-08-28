# Studio UI/UX iteration — studio-ux-02-gui-viewport-stub

**UTC:** 2026-05-24T16:24:00Z  
**Branch:** cursor/studio-ui-ux-plan-loop

## UX dimensions (0–3)

| ID | Score | Note |
|----|------:|------|
| UX-01 | 2 | ViewportRegion + grid paint op; pixels still HTML mock. |
| UX-02 | 2 | Timeline in layout; no native playhead. |
| UX-03 | 2 | Inspector stroke in paint expansion. |
| UX-04 | 2 | Command palette mock only. |
| UX-05 | 2 | Profile chip HTML only. |
| UX-06 | 2 | Agent strip in paint IR; agentic chrome mock. |
| UX-07 | 2 | Empty viewport mock + region sizing. |
| UX-08 | 1.5 | No GPU-fail strip. |
| UX-09 | 2 | Panel switch timing hooks (<100 ms budget). |
| UX-10 | 2 | Token contrast; axe when harness runs. |
| UX-11 | 1.5 | No skeleton loaders. |
| UX-12 | 2 | Consistent terminology. |
| UX-13 | 2 | Seven paint cmds + timing struct. |
| UX-14 | 3 | Mock labeled; gui stub honest. |

## PH-UX gates

```json
{
  "viewport_fps_target": 60,
  "panel_switch_ms_target": 100,
  "panel_switch_hook": "gui_panel_switch_within_budget",
  "particle_tiers": "skip (lic missing)"
}
```

## Regressions

None vs studio-ux-01. UX-09 improved (1.5 → 2).

## SOTA refs (agentic_ai + viewport)

- Cursor — agent task status, cancel, error strip patterns
- Linear — command palette latency + panel transitions
- Blender — viewport region + keyboard-first panel focus
