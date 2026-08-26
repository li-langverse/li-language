# Studio gap-close wave1 — all smokes pass `lic check`

## Summary

`packages/li-studio` library and smoke tests compile under `lic check` on `feat/studio-gap-close-wave1` (timeline extern, move semantics, MCP/viewport/loading contracts).

## Agent continuation

1. **Read** — `packages/li-studio/src/lib.li`; `packages/li-studio/li-tests/manifest.toml`; `scripts/patch-studio-lib-final.py` (regenerate lib from `532cf31` base if rebased).
2. **Run** — `./build/compiler/lic/lic check packages/li-studio/src/lib.li`; loop `lic check packages/li-studio/li-tests/smoke/<name>.li`.
3. **Then** — merge PR after CI; continue PH-UX / PH-AGENT runtime wiring in `runtime/li_rt.c` if smokes need live MCP/timeline state.
4. **Blocked on** — human merge; wgpu viewport failure probe (UX-08 still mock-only).

## Changed

- `packages/li-studio/src/lib.li` — `li_rt_studio_timeline_*` extern block; `raises IO` on timeline/viewport wrappers; inspector field row offsets without int×float; E0311 copy fixes in compose paths; `studio_inspector_field_at` ensures; MCP `ensures result != ""`; shell loading 6-arg + `w0/h0/w1/h1`; `studio_compose_shell_profile` layout borrow via `lay`.
- `packages/li-studio/li-tests/smoke/studio_profile_roundtrip.li` — separate `compose_chk` / `compose_paint` (move semantics).
- `packages/li-studio/li-tests/smoke/studio_viewport_error.li` — fresh layout/overlay/shell per check (move semantics).
- `packages/li-studio/li-tests/manifest.toml` — `studio_mcp_tools.li` smoke entry (if missing).
- `scripts/patch-studio-lib-final.py` — reproducible lib patch from git base `532cf31`.

## Not changed

- `runtime/li_rt.c` behavior beyond existing symbols (no new MCP server).
- `packages/li-render`, `packages/li-gpu`, compiler codegen.
- Master plan PH phase ordering or benchmark thresholds.
- SDL/wgpu windowed studio demo (headless smokes only).

## Breaking

N/A — compile/proof contract fixes only; public API names unchanged.

## Security

N/A — no CVE catalog or trusted.lean changes.

## Performance

N/A — no benchmark row; compose/paint smoke compile only.

## Downstream

N/A — `li_std_studio_version()` remains `5`; consumers re-run `lic check` on studio imports after pull.
