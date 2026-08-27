# Vertical gap #5: `lig-kernels.toml` catalog + parity harness

**Summary:** Expand `benchmarks/competitive/lig-kernels.toml` to all `lig.kernel.*` catalog IDs (including `md_force_short` and `heat_stencil_2d_f32` stubs), document `cuda`/`hip`/`metal` as `N/A` until emit env, and fix `bench-lig-kernel-parity.sh` to emit every registry `kernel_id` while keeping matmul smoke green.

## Agent continuation

1. **Read** — `docs/game-dev/lig-kernel-catalog.md`, `benchmarks/competitive/lig-kernels.toml`, `scripts/bench-lig-kernel-parity.sh`, `packages/lig/li-tests/smoke/kernel_matmul_parity.li`.
2. **Run** — `./scripts/bench-lig-kernel-parity.sh`; build matmul smoke from `packages/lig/`; `./li-tests/run_all.sh smoke` (expects `PASS compile_open_ok packages/lig/li-tests/smoke/kernel_matmul_parity.li`).
3. **Then** — WP2: wire `import lig` for package smokes; fill vendor columns when `LIG_EMIT_CUDA|HIP|METAL=1` lowering lands.
4. **Blocked on** — LKIR vendor emit (no timed `cuda`/`hip`/`metal` in TOML until then).

## Changed

| Area | What | Evidence |
|------|------|----------|
| Registry | 14 `[[kernel]]` rows aligned to catalog; meta `emit_env_*` | `benchmarks/competitive/lig-kernels.toml` |
| Vendor honesty | `cuda`/`hip`/`metal` = `N/A` on all rows | catalog + TOML header comments |
| Harness | JSON `kernel_ids` + per-kernel stub/pilot entries | `./scripts/bench-lig-kernel-parity.sh` |
| Smoke | Standalone matmul pilot FFI smoke | `T-PKG-lig-kernel-matmul-parity` compile_open_ok |
| li-tests | Resolve `packages/*` manifest paths via `$REPO` | `li-tests/run_all.sh` |

## Not changed

- LKIR lowering / `LIG_EMIT_*` codegen — no compiler emit in this PR.
- `import lig` package resolution from repo root — smoke uses inlined FFI shims.
- `benchmarks/competitive/registry.toml` tier-1/2 CPU rows.
- benchmarks dashboard vendor ratio ingest — N/A until emit env.

## Breaking

None.

## Security

N/A — registry metadata and smoke compile only.

## Performance

N/A — stub registry; matmul pilot timing in local JSON only.

## Downstream

| Repo | Action |
|------|--------|
| benchmarks dashboard | Ingest expanded registry when vendor emit fills columns |
