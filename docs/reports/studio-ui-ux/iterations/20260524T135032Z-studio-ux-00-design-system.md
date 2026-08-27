# Studio UI/UX iteration — `studio-ux-00-design-system`

_UTC 2026-05-24T13:50:32.970857+00:00_

## UX assessment

- **pass:** True
- **avg_score:** 2.07
- **min_score:** 1.5

| ID | Score | Note |
|----|------:|------|
| UX-01 | 2 | Workspace mock: grid, selection ring, HUD depth cues; native viewport still stubbed (Godot/Blender bar = 3). |
| UX-02 | 2 | Timeline track + playhead in workspace mock; empty timeline affordance in 02 mock (DaVinci/Unreal = 3). |
| UX-03 | 2 | Inspector kv grid readable at 320px; density OK for drug-design profile (Unity/Figma = 3). |
| UX-04 | 2 | ⌘K hint visible on both mocks; palette not interactive (Linear/VS Code = 3). |
| UX-05 | 2 | Profile chip shows vertical; switching not implemented (Notion DB switch = 3). |
| UX-06 | 2 | Agent strip: running+cancel vs idle (Cursor/Copilot/Linear agent patterns); mock only. |
| UX-07 | 2 | 02-empty mock: no scene, no selection, idle agent (shadcn empty-state pattern). |
| UX-08 | 1.5 | No GPU-fail strip yet; placeholder copy only (Primer recovery = 3). |
| UX-09 | 1.5 | Focus rings on dock/cancel/open; no global shortcuts doc (Blender/Linear = 3). |
| UX-10 | 2 | Token contrast AA on mocks; ux-harness axe pass on world-studio-demo. |
| UX-11 | 1.5 | No skeleton loaders; motion tokens define panel/hover budget (Material 3 = 3). |
| UX-12 | 2 | Consistent Li World Studio / AMBER terminology across mocks (Diátaxis tone). |
| UX-13 | 2 | HUD labels FPS as mock + particle tier; honest about wgpu gap. |
| UX-14 | 3 | Mock banner on every HTML surface; no native mislabel. |

## Bench

```json
{
  "generated_at": "2026-05-24T13:49:43Z",
  "load_ms": 0.05,
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
    "skip_md_bench:lic_or_harness_missing"
  ]
}
```

