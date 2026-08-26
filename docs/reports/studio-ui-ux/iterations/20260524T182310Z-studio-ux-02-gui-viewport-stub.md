# Studio UI/UX iteration — `studio-ux-02-gui-viewport-stub`

_UTC 2026-05-24T18:23:10.225223+00:00_

## UX assessment

- **pass:** True
- **avg_score:** 2.11
- **min_score:** 1.5

| ID | Score | Note |
|----|------:|------|
| UX-01 | 2 | ViewportRegion + grid paint op from gui_paint_studio_shell_chrome; pixels still HTML mock (Godot/Blender = 3). |
| UX-02 | 2 | Timeline region in layout IR; panel switch hooks do not animate playhead yet (DaVinci/Unreal = 3). |
| UX-03 | 2 | Inspector stroke in expanded paint IR; native kv grid pending (Unity/Figma = 3). |
| UX-04 | 2 | Command palette still mock hint only (Linear/VS Code = 3). |
| UX-05 | 2 | Profile chip in HTML; vertical switch not in composables (Notion = 3). |
| UX-06 | 2 | Agent strip fill in paint expansion; task/cancel chrome mock-only (Cursor agent UIs = 3). |
| UX-07 | 2 | Empty viewport mock; ViewportRegion supports zero-scene shell sizing. |
| UX-08 | 1.5 | No GPU-fail recovery strip in gui layer yet (Primer = 3). |
| UX-09 | 2 | gui_panel_switch_to + within_budget hooks wired; no global shortcut map (Blender/Linear = 3). |
| UX-10 | 2 | Token contrast unchanged; ux-harness axe on world-studio-demo when harness runs. |
| UX-11 | 1.5 | No skeleton loaders in paint IR (Material 3 = 3). |
| UX-12 | 2 | Consistent Studio terminology in gui/ui APIs. |
| UX-13 | 2 | Seven paint cmds + panel timing struct; bench tiers skip honestly without lic. |
| UX-14 | 3 | Mock banner preserved; li-gui hooks documented as stub, not native viewport pixels. |

## Bench

```json
{
  "generated_at": "2026-05-24T18:22:46Z",
  "load_ms": 0.07,
  "viewport_fps_target": 60,
  "panel_switch_ms_target": 100,
  "particle_tiers": [
    {
      "id": "md_1000",
      "particles": 1000,
      "fps_target": 60,
      "status": "skip"
    },
    {
      "id": "md_10000",
      "particles": 10000,
      "fps_target": 60,
      "status": "skip"
    },
    {
      "id": "md_100000",
      "particles": 100000,
      "fps_target": 30,
      "status": "skip"
    }
  ],
  "memory_mib": {
    "profile_exit": 0,
    "lines": [
      "tracemalloc peak (import): 1.07 MiB"
    ]
  },
  "notes": [
    "present:li-ui",
    "present:li-gui",
    "skip_md_bench:lic_or_harness_missing"
  ]
}
```

