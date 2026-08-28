# Studio UI/UX iteration — studio-ux-02-gui-viewport-stub

**UTC:** 2026-05-24T17:33:00Z  
**Branch:** cursor/studio-ui-ux-plan-loop  
**Capture issue:** https://github.com/li-langverse/lic/issues/180#issuecomment (latest comment)

## UX dimensions (0–3)

| ID | Score | Note |
|----|------:|------|
| UX-01 | 2 | ViewportRegion + grid paint op; pixels still HTML mock (Godot/Blender = 3). |
| UX-02 | 2 | Timeline in layout IR; no native playhead (DaVinci/Unreal = 3). |
| UX-03 | 2 | Inspector stroke in paint expansion (Unity/Figma = 3). |
| UX-04 | 2 | Command palette mock only (Linear/VS Code = 3). |
| UX-05 | 2 | Profile chip HTML only (Notion = 3). |
| UX-06 | 2 | Agent strip in paint IR; task/cancel chrome mock (Cursor/Copilot = 3). |
| UX-07 | 2 | Empty viewport mock + ViewportRegion sizing. |
| UX-08 | 1.5 | No GPU-fail strip (Primer = 3). |
| UX-09 | 2 | `gui_panel_switch_to` + `gui_panel_switch_within_budget` hooks (<100 ms). |
| UX-10 | 2 | Token contrast; ux-harness axe pass on world-studio-demo. |
| UX-11 | 1.5 | No skeleton loaders (Material 3 = 3). |
| UX-12 | 2 | Consistent Studio terminology in gui/ui APIs. |
| UX-13 | 2 | Seven paint cmds + timing struct; bench tiers skip honestly. |
| UX-14 | 3 | Mock banner; li-gui documented as stub not native pixels. |

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

- **cursor-agent** — agent status, tool progress, cancel (ux-harness agentic_ai)
- **linear-app** — command palette latency, fast panel transitions
- **godot-editor** — viewport region clarity
