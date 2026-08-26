# Studio UI/UX iteration — `studio-ux-10-native-capture`

- **Branch:** `cursor/studio-ui-ux-plan-loop`
- **Native:** `deploy/studio-demo/native/studio_viewport_capture.c` + `scripts/studio-ui-ux-capture-native.sh`
- **Harness:** `li-cursor-agents` `ux-harness/adapters/native_capture.py`, target `world-studio-native`

## Changes

- SDL2 viewport stub (grid, particles, selection HUD) captured under Xvfb when `libsdl2-dev` + `xvfb-run` present
- `studio-ui-ux-capture-progress.sh` calls native capture before HTML mocks; sets `native_pixels` from `latest-native-capture.json`
- `studio-ui-ux-verify-native-capture.py` gate (soft-skip without SDL)

## Gap (this runner)

- `libsdl2-dev` not installed — native capture skipped; `honest_native_viewport=false`
