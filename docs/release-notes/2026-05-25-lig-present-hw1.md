# lig.present — PH-HW HW-1 swapchain / present path

## Summary

Adds `packages/lig` (`import lig.present`) with swapchain create and present contracts on a documented wgpu/SDL trusted FFI edge, wires `li-render` host FPS ticks and `li-studio` shell input/present glue, and moves the wgpu smoke bench hook to `packages/lig/bench/wgpu_smoke.toml`.

## Agent continuation

1. **Read:** `packages/lig/README.md`, `docs/game-dev/studio-shell-input-bridge.md`, `runtime/li_rt.c` (`li_rt_lig_*`, `li_rt_studio_shell_input_*`).
2. **Run:** `lic check packages/lig/li-tests/smoke/wgpu_smoke.li packages/li-render/li-tests/smoke/viewport_fps_counter.li packages/li-studio/li-tests/smoke/studio_host_present.li`; on aarch64-apple-darwin with SDL2: `LIG_HOST_PRESENT=1 ./scripts/studio-shell-present-tick.sh`.
3. **Then:** Stack PR `feat/ph-hw-wp3-present` onto `feat/ph-hw-multi-vendor`; WP4 LKIR device tables only after human review of trusted-edge policy.
4. **Blocked on:** in-tree wgpu-rs crate and real GPU adapter enumeration (still runtime mock / SDL present stub).

## Changed

| Area | Paths / IDs |
|------|-------------|
| **lig.present** | `packages/lig/src/lib.li`, `bench/wgpu_smoke.toml`, smokes `T-PKG-lig-wgpu-smoke` |
| **li-render** | `render_present_viewport_tick`, `render_fps_counter_host_tick`, `li_std_render_version` → 3 |
| **li-studio** | `studio_shell_input_from_host`, `studio_shell_host_frame`, `li_std_studio_version` → 6, smoke `studio_host_present.li` |
| **Runtime** | `runtime/li_rt.c`, `runtime/li_rt.h`, `compiler/codegen/emit.cpp` |
| **Native** | `deploy/studio-demo/native/studio_shell_present_host.c`, `scripts/studio-shell-present-tick.sh` |
| **Bench** | `scripts/bench-studio-viewport-perf.sh` prefers `packages/lig/bench/wgpu_smoke.toml` |

## Not changed

- LKIR kernels, VRAM budgets, and `li-gpu` CUDA/ROCm launch paths (PH-HW WP4+).
- Shipped `li-studio` binary / full wgpu-rs dependency graph.
- `packages/li-gpu/src/lib.li` backend ID constants (still authoritative for `gpu_backend_*`).
- Tier-2 physics benches and httpd Phase-H packages.

## Breaking

N/A — new package and additive runtime symbols; studio smokes require `li_std_studio_version() >= 6`.

## Security

N/A — no new network surface; trusted FFI documented in `packages/lig/README.md` (same policy class as existing `li_rt_studio_*` mocks).

## Performance

N/A — bench hooks remain simulate-first; `LIG_HOST_PRESENT=1` enables host `dt_ms` for FPS counter (see `packages/li-render/bench/viewport_fps.toml`). No dashboard row yet.

## Downstream

- **feat/ph-hw-multi-vendor** PR should merge before this branch; rebase if `packages/lig` WP2 lands separately.
- **benchmarks** dashboard: optional wgpu_smoke row after human enables native CI on `aarch64-apple-darwin`.
