# Studio UI/UX iteration — `studio-ux-03-render-wgpu-smoke`

_UTC 2026-05-24T21:42:00Z — gates/capture/bench re-verified_

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

**SOTA (agentic_ai):** [Cursor agent](https://cursor.com/docs/agent/overview) (task/status), [Linear](https://linear.app/) (panel/progress), [GitHub Copilot](https://docs.github.com/en/copilot) (error recovery).

## Bench

- **load_ms:** 0.09
- **viewport_fps:** 60.0 estimated, meets_target=true, wgpu_smoke=stub_pass, surface_ok=false
- **md_particles:** skip (lic/harness)
- **memory_mib:** 1.07 peak (profile-animate-memory)

## Capture

- **exit:** 0
- **issue comment:** https://github.com/li-langverse/lic/issues/181#issuecomment-4530036685
- **gap:** HTML headless PNG timed out (45s); ux-harness mock pass; native_pixels=false
