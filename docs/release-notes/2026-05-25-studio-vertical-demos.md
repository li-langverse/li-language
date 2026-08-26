# Studio per-vertical native demo capture (UX-14)

## Summary

Per-vertical examples and a **native-only** `studio-verticals-demo.mp4` — HTML vertical mocks archived; `record-studio-verticals-demo.sh` exits 1 without native frames.

## Agent continuation

1. **Read** — `docs/demo/VERTICALS-RECORDING.md`, `packages/li-studio/examples/verticals/`, `deploy/studio-demo/archive/README.md`.
2. **Run** — `lic check packages/li-studio/li-tests/smoke/studio_vertical_profile_roundtrip.li`; `LIG_HOST_PRESENT=1 ./scripts/record-studio-verticals-demo.sh`.
3. **Next** — Wire `STUDIO_DEMO_PROFILE` into `studio_shell_demo_frame` / `li-studio-demo` window capture; retire CPU framebuffer stub when wgpu present is stable.
4. **Blocked** — Full domain packs (`li-sim-automotive`, export.print, adaptive drug GUI) remain PH-SIM / PH-GD follow-ups.

## Changed

| Path | Notes |
|------|-------|
| `packages/li-studio/examples/verticals/*/` | `studio.toml` + README per profile |
| `packages/li-studio/li-tests/smoke/studio_vertical_profile_roundtrip.li` | T-PKG-li-studio-vertical-profile-roundtrip |
| `deploy/studio-demo/native/studio_verticals_present_host.c` | Per-profile native frames (`native_pixels=1`) |
| `deploy/studio-demo/native/studio_shell_present_host.c` | `LIG_HOST_PRESENT` present tick |
| `scripts/studio-verticals-capture-native.sh` | Cycle 7 profiles → PNG dir |
| `scripts/record-studio-verticals-demo.sh` | Native-only MP4; **exit 1** on missing native dir |
| `deploy/studio-demo/archive/verticals-html-mocks/` | HTML mocks — **not for user demo** |
| `docs/demo/VERTICALS-RECORDING.md` | Honesty matrix + blocker repro |
| `scripts/studio-ppm-to-png.py` | Fix P6 header parse for large frames |

## Not changed

- `studio_profile_*` id table / runtime `li_rt.c` mapping
- Full wgpu swapchain product capture from shipped `li-studio-demo` binary
- `li-langverse/studio` org repo sync

## Breaking

N/A — demo capture path stricter (no silent HTML fallback).

## Security

N/A — local native capture; archived static HTML not used in user MP4.

## Performance

N/A — no bench threshold changes.

## Downstream

- **studio** org repo: mirror `examples/verticals` when syncing from lic.
