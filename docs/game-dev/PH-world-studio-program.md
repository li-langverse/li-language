# PH-world-studio-program — master tracker

**Status:** Planning  
**Vision:** [world-studio-vision.md](world-studio-vision.md)

Cross-cutting program IDs. Implementation order respects dependencies in the vision doc.

| Program | Phases | Depends on |
|---------|--------|------------|
| **PH-GD** | GD-0…7 | `li-scene`, `li-ui` |
| **PH-UX** | UX-0…5 | PH-GD-1 |
| **PH-SIM** | SIM-0…6 | `li-physics-runtime` |
| **PH-ROBO** | ROBO-0…5 | PH-SIM-1 |
| **PH-AM** | AM-0…9 | PH-SCI-2, PH-UX-3 |
| **PH-SCI** | SCI-0…7 | tier-2 physics |
| **PH-DRUG** | DRUG-0…7 | PH-SCI-2, PH-GD-1, PH-AGENT |
| **PH-QM** | QM-0…7 | PH-HW, PH-COMPLY |
| **PH-VOXEL** | VOXEL-0…5 | PH-GD-5 |
| **PH-PUB** | PUB-0…5 | PH-UX, `sim.viz` |
| **PH-ML** | ML-0…5 | PH-HW-1 |
| **PH-AGENT** | AGENT-0…6 | `lic check --format=json` |
| **PH-PORT** | PORT-0…2 | LLVM triples |
| **PH-HW** | HW-0…4 | `lig` (WP1 governance; WP2+ `packages/lig`) |
| **PH-COMPLY** | COMPLY-0…4 | governance |

**Next execution milestones:** RFC stubs (landed) → `li-studio` scaffold (PH-GD-1) → **PH-SIM SIM-1** tick stub (landed) → PH-SIM SIM-2 replay → SIM-3 RL `EnvPool` hookup.

**PH-SIM SIM-1 (landed):** `sim_reset` / `sim_step` on `SimSessionStub` (deterministic `tick`, no physics); `studio_sim_step_hook` after SIM-0 profile bridge. Evidence: `packages/li-sim/li-tests/smoke/sim_step_stub.li`, `docs/release-notes/2026-05-25-sim-step-sim1-stub.md`.
