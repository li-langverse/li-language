# Release notes: 2026-05-25 — studio-sim-profile-bridge

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/studio-gap-close-wave1`  
**PH / REQ:** PH-SIM SIM-0, PH-UX-05  
**Author:** agent

---

## Summary (one sentence)

PH-SIM SIM-0 read-only bridge: map `studio_profile_*` ids to `sim_contract_*`, apply `StudioProjectConfig` onto `SimSessionStub` (`output_detail` + contract id) without stepping.

## Agent continuation (required)

1. Read: `packages/li-sim/src/lib.li` (SIM-0 block), `packages/li-studio/src/lib.li` (`studio_apply_profile_to_sim`), `docs/game-dev/specs/li-engine-unified-sim-rfc.md`.
2. Run: `lic check packages/li-sim/src/lib.li`; `lic check packages/li-sim/li-tests/smoke/studio_profile_bridge.li`; `lic check packages/li-studio/src/lib.li`.
3. Then: PH-SIM SIM-1 — `sim_reset` / `sim_step` on `SimWorld` per unified-sim RFC (no Studio duplicate physics).
4. Blocked on: full engine tick / lis MCP `sim_set_profile` persistence — **none** for this merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `packages/li-sim` | `sim_contract_*`, `li_sim_profile_from_studio_id`, `SimSessionStub`, `sim_session_apply_studio_profile` | `li-tests/smoke/studio_profile_bridge.li` |
| `packages/li-studio` | `import sim`, `studio_apply_profile_to_sim`; `studio_profile_sim_robotics` → `4` (matches `li_rt`) | smoke + composable |
| `packages/li-studio/li.toml` | path dep `li-sim` | package resolve |
| `li-tests/composable` | `import_studio_sim_profile_bridge.li` | monorepo manifest |
| `docs/release-notes/2026-05-25-studio-sim-profile-bridge.md` | this file | agent continuation |

## Not changed (scope fence)

- `sim_reset` / `sim_step` / `SimWorld` — **not** implemented (SIM-1).
- lis MCP HTTP `sim_set_profile` file write — **not** wired.
- Tier-2 bench summaries / `run_simulation` stepping — **not** changed.
- LLVM / httpd / tier5 — **not** touched.

## Breaking / Security / Performance / Downstream

- **Breaking:** `studio_profile_sim_robotics()` constant corrected from `5` to `4` (aligned with `runtime/li_rt.c`); callers hard-coding `5` for robotics must update.
- **Security:** N/A — read-only int mapping, no I/O.
- **Performance:** N/A — O(1) field writes on stub.
- **Downstream:** `li-studio` now depends on `li-sim`; composable agents should import both for profile bridge smokes.

## CHANGELOG entry (paste into Unreleased)

```markdown
### Added
- **PH-SIM SIM-0 studio→sim profile bridge** — `li_sim_profile_from_studio_id`, `SimSessionStub`, `studio_apply_profile_to_sim`; smokes `studio_profile_bridge.li`, `import_studio_sim_profile_bridge.li` — [2026-05-25-studio-sim-profile-bridge.md](docs/release-notes/2026-05-25-studio-sim-profile-bridge.md).
```
