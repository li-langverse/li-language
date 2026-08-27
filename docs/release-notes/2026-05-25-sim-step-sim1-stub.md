# Release notes: 2026-05-25 — sim-step-sim1-stub

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**Branch:** `feat/ph-hw-sim1` → PR into `feat/ph-hw-multi-vendor`  
**PH / REQ:** PH-SIM SIM-1  
**Author:** agent

---

## Summary (one sentence)

PH-SIM SIM-1 adds `sim_reset` / `sim_step` on `SimSessionStub` (deterministic tick counter, no physics kernel) plus `studio_sim_step_hook` to sync studio profile then step.

## Agent continuation (required)

1. Read: `packages/li-sim/src/lib.li` (SIM-1 block), `packages/li-studio/src/lib.li` (`studio_sim_step_hook`), `docs/game-dev/specs/li-engine-unified-sim-rfc.md`.
2. Run: `lic check packages/li-sim/src/lib.li`; `lic check packages/li-sim/li-tests/smoke/sim_step_stub.li`; `lic check packages/li-studio/src/lib.li`; `lic check li-tests/composable/import_studio_sim_step_stub.li`.
3. Then: PH-SIM SIM-2 — replay buffers / checkpoint hooks on session or `SimWorld` per RFC.
4. Blocked on: `li-physics-runtime` integrators, lis MCP tick persistence — **none** for this merge.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| `packages/li-sim` | `sim_status_*`, `SimSessionStub.tick`/`last_dt`, `sim_reset`, `sim_step` | `li-tests/smoke/sim_step_stub.li` |
| `packages/li-studio` | `studio_sim_step_hook` (profile apply + `sim_step`) | smoke + composable |
| `li-tests/composable` | `import_studio_sim_step_stub.li` | monorepo manifest |
| `docs/release-notes/2026-05-25-sim-step-sim1-stub.md` | this file | agent continuation |

## Not changed (scope fence)

- `SimWorld` / scene entity mutation / `li-physics-runtime` stepping — **not** implemented.
- `ml.rl.EnvPool` → `sim_step` wiring — **not** implemented (SIM-3).
- Replay / checkpoint (SIM-2) — **not** implemented.
- lis MCP HTTP tick persistence — **not** wired.
- LLVM / httpd / tier5 — **not** touched.

## Breaking / Security / Performance / Downstream

- **Breaking:** None — new fields on `SimSessionStub` default to `tick=0`, `last_dt=0.0`.
- **Security:** N/A — in-memory int/float counter, no I/O.
- **Performance:** N/A — O(1) increment per step.
- **Downstream:** Studio timeline playback can call `studio_sim_step_hook` when wiring real ticks; RL agents should wait for SIM-3.

## CHANGELOG entry (paste into Unreleased)

```markdown
### Added
- **PH-SIM SIM-1 sim step stub** — `sim_reset` / `sim_step` on `SimSessionStub`, `studio_sim_step_hook`; smokes `sim_step_stub.li`, `import_studio_sim_step_stub.li` — [2026-05-25-sim-step-sim1-stub.md](docs/release-notes/2026-05-25-sim-step-sim1-stub.md).
```
