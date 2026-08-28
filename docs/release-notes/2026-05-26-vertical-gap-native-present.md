# Vertical gap native present — STUDIO_DEMO_PROFILE + demo capture

## Summary

Wires `STUDIO_DEMO_PROFILE` into `studio_shell_demo_frame` / `studio_vertical_demo_frame`, restores `li_rt_lig_*` host present runtime dropped in HW-0 merge, and prefers `li-studio-demo` in verticals capture with documented wgpu readback blockers.

## Agent continuation

1. **Read** — `docs/demo/VERTICALS-RECORDING.md`, `docs/demo/media/README.md`, `packages/li-studio/src/lib.li`, `runtime/li_rt.c`.
2. **Run** — `lic check packages/li-studio/li-tests/smoke/studio_vertical_demo_env.li`; `LIG_HOST_PRESENT=1 ./scripts/studio-verticals-capture-native.sh`.
3. **Then** — in-tree wgpu-rs + swapchain readback from presented viewport.
4. **Blocked on** — wgpu-rs crate graph and `RenderReadPixels` FFI.

## Changed

| Area | Paths |
|------|-------|
| Studio demo | `packages/li-studio/src/lib.li`, `src/main.li`, smoke `studio_vertical_demo_env.li` |
| Runtime | `runtime/li_rt.c`, `runtime/li_rt.h` |
| Capture | `scripts/studio-verticals-capture-native.sh`, `studio_verticals_present_host.c` |
| Docs | `docs/demo/VERTICALS-RECORDING.md`, `docs/demo/media/README.md` |

## Not changed

- Full wgpu-rs readback (gap #2 remainder), other vertical-gap branches (sim packs, bench lig).

## Breaking

N/A — additive env vars and restored HW-1 symbols.

## Security

N/A — `system()` only when `STUDIO_SHELL_PRESENT_HOST_BIN` set by capture scripts.

## Performance

N/A

## Downstream

- Optional CI artifact / release upload per `docs/demo/media/README.md`.
