# PH-GD-1: Studio scene outliner stub

## Summary

Adds a static scene-hierarchy outliner in the left dock strip below tool slots (`packages/li-studio`).

## Agent continuation

1. Read `packages/li-studio/src/lib.li` — `StudioOutlinerCompose`, `studio_compose_outliner`, `studio_paint_outliner`.
2. Run `lit test` / package smoke for `li-studio` (`studio_outliner.li`, `studio_compose_panels.li`).
3. Next: wire `li-scene` `Scene` into outliner compose (replace demo nodes); keep `node_count <= 8` contract.
4. Blocked: none for this slice.

## Changed

- `packages/li-studio/src/lib.li` — outliner types, compose, paint; `StudioShellCompose.outliner`; `li_std_studio_version` → 5.
- `packages/li-studio/li-tests/smoke/studio_outliner.li`, `studio_compose_panels.li`, `manifest.toml`.
- `packages/li-studio/README.md` — Compose API bullet.

## Not changed

- `li-scene` graph import, viewport picking, timeline playback semantics beyond existing UX-02 stubs.
- `li-ui` shell layout regions; dock width unchanged.

## Breaking

N/A — additive compose field on `StudioShellCompose`; smoke tests gate version 5.

## Security

N/A — no trusted surface or host I/O.

## Performance

N/A — +3 stroke paint cmds for demo nodes; no bench delta required.

## Downstream

N/A — `li-gui` / `li-render` unchanged; studio hosts must read `compose.outliner` if painting outside `studio_paint_shell_chrome`.
