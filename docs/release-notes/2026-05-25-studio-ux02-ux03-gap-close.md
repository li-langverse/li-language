# Studio UX-02/03 gap close (li-studio)

## Summary

Native timeline playback toggle/tick and inspector selection field rows in `li-studio`, with smokes and runtime playhead state.

## Agent continuation

1. Read `packages/li-studio/src/lib.li` — `studio_timeline_*`, `StudioInspectorField`, `studio_compose_shell(..., scene_entity_count)`.
2. Run `lic check packages/li-studio` (or package smokes via `lit`) after pulling `feat/studio-gap-close-wave1`.
3. Next: wire host tick to `studio_timeline_tick_frame` in viewport loop; optional UX-07 viewport empty paint in shell if not already merged upstream.
4. Blocked: none for compose/paint layer.

## Changed

- `packages/li-studio/src/lib.li` — `li_std_studio_version` 5; timeline RT + compose/paint; inspector fields; restored `studio_compose_shell_palette` with `scene_entity_count`.
- `runtime/li_rt.c`, `runtime/li_rt.h`, `compiler/codegen/emit.cpp` — timeline mock state.
- `packages/li-studio/li-tests/smoke/studio_timeline_playback.li`, `studio_inspector_fields.li`; manifest + panel/cmd count updates.

## Not changed

- `li-ui` layout tokens, `li-gui` keyboard routing, wgpu render path, agent MCP contracts.

## Breaking

N/A — additive API; `studio_compose_shell` already takes `scene_entity_count` on this branch.

## Security

N/A — UI compose only; no new trusted surface.

## Performance

N/A — deterministic +0.01 playhead step per tick; no bench delta claimed.

## Downstream

Branches consuming `li-studio` should bump version gate to `li_std_studio_version() >= 5` for new smokes.
