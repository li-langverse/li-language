# Release notes: 2026-05-25 — vertical-gap-sim-step-physics

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/vertical-gap-sim-step-physics` → PR into `main`  
**PH / REQ:** PH-SIM SIM-1, vertical gaps **#4** (domain step stubs), **#9** (MD tier metadata)  
**Author:** agent

---

## Summary (one sentence)

Studio timeline stepping calls `sim.scientific` MD smoke on `sim_scientific` profile and `physics.runtime` `physics_step` on `game`, with `studio_md_particle_tier_select_ok` wired to `li-scene` tier metadata.

## Agent continuation (required)

1. Read: `packages/li-studio/src/lib.li` (`studio_sim_step_hook`, `studio_game_step_hook`, `studio_md_particle_tier_select_ok`), `packages/li-sim-scientific/src/lib.li` (`sim_scientific_tick_stub`), `packages/li-scene/src/lib.li` (MD tier types).
2. Run: `lic check --workspace`; `lic check packages/li-studio/li-tests/smoke/studio_sim_step_by_profile.li`; `lic check li-tests/composable/import_studio_sim_step_by_profile.li`.
3. Then: SIM-2 replay buffers; full `SimWorld` + scene sync for `physics_sync_*`; tier-2 MD oracle benches for `sim_scientific` viewport.
4. Blocked on: lis MCP tick persistence, `EnvPool` RL hookup (SIM-3) — **none** for this merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `packages/li-sim-scientific` | `sim_scientific_tick_stub` → `run_md_lj_smoke` | composable + studio smoke |
| `packages/li-studio` | deps `li-sim-scientific`, `li-scene`, `li-physics-runtime`; profile step hooks | `studio_sim_step_by_profile.li` |
| `li-tests/composable` | `import_studio_sim_step_by_profile.li` | monorepo manifest |
| `docs/release-notes/2026-05-25-vertical-gap-sim-step-physics.md` | this file | agent continuation |

## Not changed (scope fence)

- `SimWorld` entity mutation / replay (SIM-2) — **not** implemented.
- `ml.rl.EnvPool` → `sim_step` (SIM-3) — **not** wired.
- `physics_sync_from_scene` / `physics_sync_to_scene` — **not** called from Studio.
- lis MCP HTTP server — **not** implemented.
- Tier-2 MD/PDE competitive oracles — **not** added (display tier metadata only).
- LLVM / httpd / tier5 — **not** touched.

## Breaking / Security / Performance / Downstream

- **Breaking:** None — new hooks; `studio_sim_step_hook` still returns `sim_status_*` codes.
- **Security:** N/A — in-process float stubs, no I/O.
- **Performance:** N/A — O(1) stub per frame; full MD bench rows separate.
- **Downstream:** Studio native timeline can call `studio_sim_step_hook`; RL/automotive profiles still use generic `sim_step` only.

## CHANGELOG entry (paste into Unreleased)

```markdown
### Added
- **Vertical gap #4/#9 sim step physics** — `sim_scientific_tick_stub`, `studio_game_step_hook`, `studio_md_particle_tier_select_ok`, profile smokes — [2026-05-25-vertical-gap-sim-step-physics.md](docs/release-notes/2026-05-25-vertical-gap-sim-step-physics.md).
```
