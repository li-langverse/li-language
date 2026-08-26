# Release notes: 2026-05-25 — li-world-scaffold

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/studio-gap-close-wave1`  
**PH / REQ:** PH-GD-2  
**Author:** agent

---

## Summary (one sentence)

PH-GD-2 scaffold: new `packages/li-world` with `world_v1` text line serialize/parse, in-memory buffer round-trip, and `li_rt_world_*` trusted seam — no scene graph or studio wiring.

## Agent continuation (required)

1. Read: `packages/li-world/README.md`, `packages/li-world/src/lib.li`, `docs/game-dev/world-studio-vision.md` §6 PH-GD-2.
2. Run: `lic check packages/li-world/src/lib.li`; `lic check packages/li-world/li-tests/smoke/world_roundtrip.li`.
3. Then: PH-GD-3 — `li-studio-ai` apply_patch loop; wire studio `world_scaffold` MCP tool to real world load when scene graph exists.
4. Blocked on: multi-line worlds, quoted names, filesystem save — **not** in this PR.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `packages/li-world` | `WorldSnapshot` (`name` slot id), `world_format_version`, serialize/parse/buffer APIs | `lic check` + `world_roundtrip.li` |
| `packages/li.toml` | workspace member `li-world` | `scripts/check-pkg-workspace.py` N/A (httpd-only gate) |
| `runtime/li_rt.c` | `li_rt_world_serialize_slot`, parse, `roundtrip_fields`, `snapshot_eq_fields` | round-trip smoke |
| `compiler/codegen/emit.cpp` | declare `li_rt_world_*` | link with world package |

## Not changed (scope fence)

- Full scene graph, entities, components — **not** implemented.
- `li-studio` / `li-scene` integration — **not** wired.
- Binary world format, filesystem trusted I/O — **not** in scope.
- MCP `world_scaffold` execution — still contract-only in `li-studio`.

## Breaking / Security / Performance / Downstream

- **Breaking:** N/A — new package.
- **Security:** In-memory string only; parse uses bounded `sscanf` name token (no spaces); no file I/O.
- **Performance:** N/A — O(1) line format for scaffold.
- **Downstream:** Studio agents should import `world` after merge; bump game-dev trackers for PH-GD-2 checkbox.
