# Studio UI/UX iteration — `studio-ux-00-design-system`

_UTC 2026-05-24T13:37:09.992353+00:00_

## UX assessment

- **pass:** True
- **avg_score:** 2.07
- **min_score:** 1.5

| ID | Score | Note |
|----|------:|------|
| UX-01 | 2 | HTML mock adds grid, selection ring, HUD depth cues; native viewport still stubbed. |
| UX-02 | 2 | Timeline track + playhead in mock; no real playback wiring. |
| UX-03 | 2 | Inspector kv grid readable at 320px; density OK for drug-design profile. |
| UX-04 | 2 | ⌘K hint visible; palette not interactive (Linear/VS Code bar = 3). |
| UX-05 | 2 | Profile chip shows vertical; switching not implemented. |
| UX-06 | 2 | Agent strip: running state + cancel (Cursor/Copilot pattern); mock only. |
| UX-07 | 2 | Inspector empty-hint for no selection detail; viewport always has scene. |
| UX-08 | 1.5 | No GPU-fail strip yet; placeholder copy only (Primer recovery = 3). |
| UX-09 | 1.5 | Focus rings on dock/cancel; no global shortcuts doc. |
| UX-10 | 2 | Token contrast AA on mock; axe pass on harness baseline. |
| UX-11 | 1.5 | No skeleton loaders; design tokens define motion budget. |
| UX-12 | 2 | Consistent Li World Studio / AMEBR terminology in mock. |
| UX-13 | 2 | HUD labels FPS as mock + particle tier; honest about wgpu gap. |
| UX-14 | 3 | Top mock-banner + HUD disclaimers; no native mislabel. |

## Bench

```json
{
  "generated_at": "2026-05-24T13:36:48Z",
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

