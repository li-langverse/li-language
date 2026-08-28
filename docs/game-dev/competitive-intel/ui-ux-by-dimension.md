# Studio UI/UX — competitive dimensions (PH-UX)

**Audience:** `studio_ui_ux_builder` plan loop · **not** httpd.

## Performance gates (PH-UX)

| Gate | Target | Measure |
|------|--------|---------|
| Viewport FPS | ≥ 60 sustained | `bench-studio-viewport-perf.sh` / future native HUD |
| Panel switch | &lt; 100 ms | Instrumented composable or mock timing |
| Studio cold load | &lt; 2 s to interactive shell | `load_ms` in bench JSON |
| MD particles (display) | 10k @ 60 fps; 100k @ 30 fps (tiered) | `md_lennard_jones` + scene path |
| Memory (animate MD) | Document peak MiB; no unbounded growth | `profile-animate-memory.sh` |

### Memory budget (animate MD / Studio timeline)

Registry: `benchmarks/competitive/studio-ui.toml` → `[[memory]] id = animate_md_import`.

| Measure | Source | Current baseline (plan loop) |
|---------|--------|----------------------------|
| Import peak | `tracemalloc` after `import animate_md` | ~5 MiB (matplotlib stack) |
| Short-run RSS | `/usr/bin/time -v` with `--skip-export --max-frames 4` | Linux CI when harness runs |
| Warn ceiling | `warn_peak_mib` in registry | **512 MiB** (gate fails above) |

**Artifacts:** `data/studio-ui-ux-plan-loop/latest-memory-profile.json` (gates + bench embed `memory_mib.profile`). Bench gate `animate_md_import` compares `peak_observed_mib` (RSS when measured, else import peak) to the warn ceiling.

**Growth policy:** `animate_md.py` streams trajectory frames from disk; full 3D GIF export can spike RAM at the matplotlib layer — document peaks honestly (UX-13) and cap preview frames in Studio (`--max-frames`). No unbounded in-memory retention of full trajectories in the harness path.

## UX dimensions (score 0–3 each iteration)

| ID | Dimension | SOTA refs |
|----|-----------|-----------|
| UX-01 | Viewport clarity (grid, selection, depth cues) | Godot, Blender |
| UX-02 | Timeline / playback affordances | DaVinci, Unreal Sequencer |
| UX-03 | Inspector density vs readability | Unity, Figma |
| UX-04 | Command palette (discoverability, latency) | Linear, VS Code |
| UX-05 | Vertical profiles (chem/bio/game) switching | Notion databases |
| UX-06 | Agentic AI: task status & cancel | Cursor, Copilot |
| UX-07 | Empty states (no scene / no selection) | shadcn patterns |
| UX-08 | Error recovery (GPU fail, missing asset) | Primer |
| UX-09 | Keyboard-first workflows | Blender, Linear |
| UX-10 | Accessibility (contrast, focus) | axe / WCAG AA |
| UX-11 | Loading / skeleton states | Material 3 |
| UX-12 | Copy & terminology consistency | Diátaxis docs tone |
| UX-13 | Performance honesty (show FPS / particle count) | Game engines |
| UX-14 | Marketing vs product truth | No HTML mock passed as native |

**Competitors (design):** Blender, Unreal Editor, Unity, Houdini (viewport); **agentic:** Cursor, Linear, GitHub Copilot Workspace.

**Evidence:** screenshots + short reel per iteration on GitHub (release `studio-ui-ux-progress`), not in git tree. Native SDL viewport frames (`deploy/studio-demo/native/`, `scripts/studio-ui-ux-capture-native.sh`, `ux-harness` `world-studio-native`) set `native_pixels=true` in `latest-capture.json` when Xvfb draws pixels; HTML mocks remain labeled marketing-only (UX-14). **UX-09:** keyboard ingest probe (`studio_shell_input_probe.c`, `scripts/studio-shell-sdl-tick.sh`, [studio-shell-input-bridge.md](../studio-shell-input-bridge.md)) maps SDL/mock keys to `InputState` for `studio_handle_studio_key` each frame.

**Bench registry:** `benchmarks/competitive/studio-ui.toml` → `./scripts/bench-studio-viewport-perf.sh` → `benchmarks/results/bench-studio-viewport-perf.json` (regenerated; plan loop also writes `data/studio-ui-ux-plan-loop/latest-bench.json`).
