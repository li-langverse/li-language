# Li Studio marketing demos (HTML only)

Static previews for the Studio UI/UX plan loop. **Not** the shipped native app (`li-ui` / `li-gui` / `li-render`).

## Screenshots

| File | Purpose |
|------|---------|
| `screenshots/01-studio-workspace.html` | Full shell: viewport, timeline, inspector, agent running |
| `screenshots/02-studio-empty-viewport.html` | Empty scene / selection / idle agent |
| `screenshots/03-studio-agent-error.html` | Agent failed + error strip (recovery UX) |
| `screenshots/studio-tokens.css` | Generated from `docs/design/studio-design-tokens.toml` |
| `screenshots/capture.sh` | Headless Chrome PNG capture (1920×1080) |
| `screenshots/verticals/` | Per-profile HTML mocks (`game`, `sim_*`) + `manifest.json` |
| `scripts/record-studio-verticals-demo.sh` | Vertical tour MP4 → `docs/demo/media/studio-verticals-demo.mp4` |

## Native viewport capture (SDL + Xvfb)

`native/studio_viewport_capture.c` draws grid + particle dots under SDL2 for honest `native_pixels` evidence when wgpu is not wired. **Not** the full `li-studio` binary.

```bash
# From lic repo root (requires libsdl2-dev; xvfb-run on headless Linux)
./scripts/studio-ui-ux-capture-native.sh   # uses STUDIO_UI_UX_NATIVE_PNG_DIR
./scripts/studio-ui-ux-verify-native-capture.py
```

`ux-harness` target `world-studio-native` invokes the same script via `native_gui` adapter.

## Native keyboard ingest (UX-09)

`native/studio_shell_input_probe.c` polls SDL keyboard/mouse once (or `--mock cmd_k,digit=3`) and prints `InputState` JSON for hosts. **Not** the full `li-studio` binary.

```bash
./scripts/studio-shell-sdl-tick.sh
STUDIO_SHELL_INPUT_MOCK=cmd_k,digit=3 ./scripts/studio-shell-sdl-tick.sh
```

See [studio-shell-input-bridge.md](../../docs/game-dev/studio-shell-input-bridge.md).

## Capture harness

```bash
# From lic repo root
STUDIO_UI_UX_ITERATION=studio-ux-10-native-capture \
  ./scripts/studio-ui-ux-capture-progress.sh
```

PNG/MP4 are written under `data/studio-ui-ux-plan-loop/artifacts/` and uploaded to GitHub release `studio-ui-ux-progress` (never committed). Progress comments go to the tracking issue (`data/studio-ui-ux-plan-loop/tracking-issue.txt`).

Dry run (gates / CI):

```bash
STUDIO_UI_UX_CAPTURE_DRY=1 ./scripts/studio-ui-ux-capture-progress.sh
./scripts/studio-ui-ux-verify-capture.py
```
