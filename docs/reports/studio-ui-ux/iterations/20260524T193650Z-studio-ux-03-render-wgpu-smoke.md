# Studio UI/UX iteration — `studio-ux-03-render-wgpu-smoke`

_UTC 2026-05-24T19:36:50Z_

## UX assessment

- **pass:** True
- **avg_score:** 2.14
- **min_score:** 1.5

| ID | Score | Note |
|----|------:|------|
| UX-01 | 2 | render_wgpu_viewport_smoke ties ViewportRegion; pixels still paint IR (Godot/Blender = 3). |
| UX-02 | 2 | Timeline region unchanged (DaVinci/Unreal = 3). |
| UX-03 | 2 | Inspector via li-gui paint (Unity/Figma = 3). |
| UX-04 | 2 | Command palette mock (Linear/VS Code = 3). |
| UX-05 | 2 | Profile chip HTML (Notion = 3). |
| UX-06 | 2 | Agent strip mock (Cursor/Copilot = 3). |
| UX-07 | 2 | Empty viewport mock + smoke size guards. |
| UX-08 | 2 | wgpu_smoke stub_pass + surface_ok=false (Primer = 3). |
| UX-09 | 2 | Panel switch hooks from studio-ux-02. |
| UX-10 | 2 | Token contrast; ux-harness pass. |
| UX-11 | 1.5 | No skeleton loaders (Material 3 = 3). |
| UX-12 | 2 | render/gpu API terminology consistent. |
| UX-13 | 3 | FPS counter + bench viewport_fps JSON; native_pixels=false. |
| UX-14 | 3 | HUD honest: bench simulate, not native wgpu surface. |

**Regressions:** none vs studio-ux-02.

**SOTA (agentic_ai):** Cursor (task/status clarity), Linear (progress affordances), GitHub Copilot Workspace (error recovery patterns).

## Bench

```json
{
  "generated_at": "2026-05-24T19:36:49Z",
  "load_ms": 0.09,
  "viewport_fps_target": 60,
  "panel_switch_ms_target": 100,
  "viewport_fps": {
    "fps_target": 60,
    "fps_estimated": 60.0,
    "meets_target": true,
    "native_pixels": false,
    "wgpu_smoke_status": "stub_pass",
    "wgpu_surface_ok": false,
    "fps_counter_hook": "li-render",
    "bench_simulate_fn": "render_bench_fps_counter_simulate",
    "hook_version": 1
  },
  "particle_tiers": [
    { "id": "md_1000", "particles": 1000, "fps_target": 60, "status": "skip" },
    { "id": "md_10000", "particles": 10000, "fps_target": 60, "status": "skip" },
    { "id": "md_100000", "particles": 100000, "fps_target": 30, "status": "skip" }
  ],
  "memory_mib": { "profile_exit": 0, "lines": ["tracemalloc peak (import): 1.07 MiB"] },
  "notes": ["present:li-ui", "present:li-gui", "present:li-gpu", "present:li-render", "skip_md_bench:lic_or_harness_missing"]
}
```

## Capture

- **exit:** 0
- **artifacts:** `data/studio-ui-ux-plan-loop/artifacts/iter-studio-ux-03-render-wgpu-smoke/` (23 PNG, ux-harness pass)
- **gap:** HTML headless screenshot timed out (45s); native pixels still stub
