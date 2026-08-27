# PH-HW HW-0: lig.device

**Summary:** Rename `li-gpu` → `lig` with runtime device probe (`li_rt_lig_*`), TOML backend line parsing, and honest viewport `native_pixels` via `lig_present_surface_ok()`.

## Agent continuation

1. **Read** — `packages/lig/README.md`, `packages/lig/src/lib.li`, `runtime/li_rt.c` (`li_rt_lig_*`), `packages/li-render/src/lib.li` (`render_wgpu_viewport_smoke`).
2. **Run** — `lic check packages/lig/li-tests/smoke/lig_device_probe.li`; `lic check packages/li-render/li-tests/smoke/viewport_fps_counter.li` when `lic` on PATH.
3. **Then** — PH-HW WP3 LKIR lowering; wire real wgpu-rs so `lig_present_surface_ok()` returns 1 when a surface presents.
4. **Blocked on** — Human merge of PR into `feat/ph-hw-multi-vendor`; no new `trusted.lean` axioms.

## Changed

| Path | Notes |
|------|--------|
| `packages/lig/` | New package (git mv from `li-gpu`); import `lig`; HW-0 API |
| `packages/li.toml` | workspace member `lig` |
| `packages/li-render/` | depends on `lig`; `RenderViewportSmoke.native_pixels` |
| `runtime/li_rt.c`, `runtime/li_rt.h` | `li_rt_lig_*` |
| `compiler/codegen/emit.cpp` | LLVM decls for `li_rt_lig_*` |
| `packages/lig/bench/device_probe.toml` | bench hook |
| `packages/lig/li-tests/smoke/lig_device_probe.li` | T-PKG-lig-device-probe |
| `scripts/bench-studio-viewport-perf.sh` | `packages/lig/bench/device_probe.toml` |

## Not changed

- LKIR kernel IR lowering (PH-HW WP3+).
- Real wgpu-rs adapter/surface creation (still stub; `lig_present_surface_ok() == 0`).
- `lic` CUDA/HIP codegen paths.
- Studio shell SDL pixel paint (still IR-only).

## Breaking

N/A — `li-gpu` / `import gpu` renamed to `lig` on integration branches that already carried `li-gpu`; update imports and `li.toml` deps.

## Security

N/A — no new trusted axioms; TOML line parser is bounded string match only.

## Performance

N/A — probe is O(1) env/platform checks; no bench threshold changes.

## Downstream

- PR base: `feat/ph-hw-multi-vendor` (when present); branch `feat/ph-hw-wp2-device`.
- `benchmarks/competitive/studio-ui.toml` hook path → `packages/lig/bench/device_probe.toml`.
