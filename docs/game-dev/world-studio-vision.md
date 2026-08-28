# Li World Studio & Li Engine — master vision

**Status:** Vision / master plan (execution started 2026-05)  
**Audience:** Architects, agents, contributors  
**Syntax:** Li uses Python-style **`def`** for functions (not `proc`). Contracts: `requires` / `ensures` / `decreases` unchanged.

**Related:** [GAME_DEV.md](../physics/GAME_DEV.md), [PH-world-studio-program.md](PH-world-studio-program.md), [competitive-landscape.md](competitive-landscape.md), [specs/](specs/), [philosophy.md](../language/philosophy.md), [master plan](../superpowers/plans/2026-05-14-li-master-plan.md)

---

## 1. One-sentence vision

**Li World Studio** is an **agent-native, provably correct** platform on **Li Engine**: one runtime for **games**, **simulation** (RL, automotive, robotics, additive manufacturing, scientific/engineering, **AI drug design**), with **killer UI/UX**, **publication-grade export**, and **Cursor SDK** agents — while **`lic build`** keeps proof first.

---

## 2. Design north star

| Pillar | Meaning |
|--------|---------|
| **One engine** | Same `li-world` · `li-scene` · `li-physics-*` · `li-render` — no CARLA/GROMACS/slicer forks |
| **AI-first** | Local models default; agents via MCP + `@cursor/sdk` |
| **Killer UX** | 60 fps, adaptive layouts, ≤3 clicks for primary flows ([PH-UX](#17-ph-ux--killer-uiux)) |
| **Research + ship** | Deterministic replay, repro bundles, `li-player` for games |
| **Read like Python** | `def`, `import`, prose names — [philosophy.md](../language/philosophy.md) |

Classic engine contrast:

| Classic | Li |
|---------|-----|
| Hidden binary scenes | `world.li` + assets (git-diffable) |
| AI bolt-ons | Agents = first-class users |
| CUDA-only ML | **CUDA + ROCm/HIP** ([PH-HW](#20-ph-hw--gpu-port--lkir)) |
| Sim ≠ game | **Profiles** on one engine ([PH-SIM](#18-li-engine--runtime-profiles)) |

---

## 3. Current baseline (lic repo)

| Layer | Package | Maturity |
|-------|---------|----------|
| Math / numerics | `li-math`, `li-math-numerics` | Expand |
| Physics | `li-physics-*`, `li-physics-runtime` | Tier-2 benches |
| Scene / UI | `li-scene`, `li-ui` | Stubs → studio chrome |
| Gameplay chem | `physics.chem` | Reactions stub — **not** QM |
| Compiler | `lic` | Proof, MIR, LLVM 22; **`def`** in all new code/docs; parser legacy bare `proc` only (do not teach) |
| Agents | `li-httpd`, `lis`, `.cursor` rules | Wire to studio MCP |

**Gaps:** `li-studio`, `li-sim`, `li-chem`, `li-voxel`, `li-render`, viewport, MCP `li-engine`, export/print pipelines.

---

## 4. Target architecture

```text
┌──────────────────────────────────────────────────────────────────────────┐
│  li-studio — killer UX, studio.adaptive, studio.publish, command palette │
│  li-studio-ai + @cursor/sdk (PH-AGENT)                                    │
├──────────────────────────────────────────────────────────────────────────┤
│  RUNTIME PROFILES: game | sim_rl | sim_automotive | sim_robotics |       │
│    sim_additive | sim_scientific | sim_drug_design                        │
├──────────────────────────────────────────────────────────────────────────┤
│  Li Engine: li-world · li-scene · li-assets · li-physics-* · li-render   │
│             li-voxel (PH-VOXEL) · li-sim (step/replay/sensors)            │
├──────────────────────────────────────────────────────────────────────────┤
│  Domain packs: automotive · robotics · additive · scientific · drug       │
│  li-chem (DFT/TDDFT/AI) · li-ml (async JobGraph)                          │
├──────────────────────────────────────────────────────────────────────────┤
│  lic build (proof) · PH-PORT targets · PH-HW CUDA/HIP/LKIR                │
└──────────────────────────────────────────────────────────────────────────┘
```

**Project files:** `world.li` + `assets/` + `studio.toml` + optional `compliance.toml` (CRITICAL packages).

---

## 5. Runtime profiles (PH-SIM)

One **`sim.step` / `game_step`** family — not duplicate physics.

| Profile | Use | Domain packs |
|---------|-----|----------------|
| `game` | Ship via `li-player` | — |
| `sim_rl` | Async env pools → engine tick | PH-ML |
| `sim_automotive` | Driving, sensors, maps | `li-sim-automotive` |
| `sim_robotics` | Arms, mobile, factory cells | `li-sim-robotics` |
| `sim_additive` | AM sim + **export to printer** | `li-sim-additive`, `sim.export.print` |
| `sim_scientific` | CFD, chem MD, heat, orbital | `li-sim-scientific`, `sim.viz` |
| `sim_drug_design` | Lab-in-the-Loop, adaptive GUI | `li-sim-drug-design`, `li-chem` |

```toml
[engine]
profile = "sim_additive"
domain = "metal_am"
determinism_tier = 2

[engine.export]
formats = ["3mf", "gcode"]
printer_profile = "profiles/bambu_x1c.toml"
require_sim_pass = true
```

RFC: [li-engine-unified-sim-rfc.md](specs/li-engine-unified-sim-rfc.md)

---

## 6. PH-GD — World Studio editor (game authoring)

| Phase | ID | Deliverable |
|-------|-----|-------------|
| 0 | PH-GD-0 | This doc + RFC index |
| 1 | PH-GD-1 | `li-studio` MVP: viewport, outliner, play/pause |
| 2 | PH-GD-2 | `li-world`, save/load text |
| 3 | PH-GD-3 | `li-studio-ai`, apply_patch loop |
| 4 | PH-GD-4 | glTF, `studio.gen` hooks |
| 5 | PH-GD-5 | `li-render` PBR-lite |
| 6 | PH-GD-6 | Scale: fluids, streaming |
| 7 | PH-GD-7 | `li-player` publish |

### Example ( `def` syntax )

```li
def game_step(world: GameWorld, input: InputState) -> unit
  requires world.time >= 0.0
  ensures player_alive(world) or world.game_over
  decreases world.tick_budget
=
  studio_poll_input(input)
  physics_step(world.physics, world.dt)
  world.tick_budget = world.tick_budget - 1
```

---

## 7. Module plan — new packages

| Import | Repo | Track |
|--------|------|-------|
| `studio` | `li-studio` | PH-GD, PH-UX |
| `studio.adaptive` | (studio) | PH-DRUG, PH-UX |
| `studio.publish` | (studio) | PH-PUB |
| `studio.ai` | `li-studio-ai` | PH-GD-3, PH-AGENT |
| `world` | `li-world` | PH-GD-2 |
| `assets` | `li-assets` | PH-GD-4 |
| `render` | `li-render` | PH-GD-5, PH-SCI |
| `player` | `li-player` | PH-GD-7 |
| `sim` | `li-sim` | PH-SIM |
| `sim.automotive` | `li-sim-automotive` | PH-SIM |
| `sim.robotics` | `li-sim-robotics` | PH-ROBO |
| `sim.additive` | `li-sim-additive` | PH-AM |
| `sim.scientific` | `li-sim-scientific` | PH-SCI |
| `sim.drug_design` | `li-sim-drug-design` | PH-DRUG |
| `chem` / `chem.dft` | `li-chem` | PH-QM |
| `voxel` | `li-voxel` | PH-VOXEL |
| `ml` | `li-ml` | PH-ML |
| `lig` | `lig` | PH-HW |

---

## 8. AI-first workflows

```text
User prompt → studio.ai / Cursor SDK → world.apply_patch
  → lic check (JSON diagnostics) → lic build → viewport / sim / export
```

**Cursor SDK:** `@cursor/sdk` local + cloud; MCP server `lis mcp li-engine`. RFC: [studio-cursor-sdk-rfc.md](specs/studio-cursor-sdk-rfc.md)

**Spin-up templates:** game, sim_rl, sim_automotive, sim_robotics, sim_additive, sim_scientific, sim_drug_design.

---

## 9. Additive manufacturing (PH-AM)

Simulate **and** manufacture:

1. Engineer mesh in scene  
2. Thermal/warp sim (`heat_equation`, proved bounds)  
3. Optional warp compensation  
4. **Export** STL / 3MF / G-code  
5. Send to printer (OctoPrint-class, trusted)

RFC: [li-sim-additive-rfc.md](specs/li-sim-additive-rfc.md)

---

## 10. Robotics (PH-ROBO)

Manipulators, mobile bases, digital twins on **same** rigid-body stack as games. Optional ROS2 bridge (trusted). RL via `sim_rl` + `sim.step`.

RFC: [li-sim-robotics-rfc.md](specs/li-sim-robotics-rfc.md)

---

## 11. Scientific & engineering sim (PH-SCI)

Graphical CFD, MD (`md_lennard_jones`), heat, fluids, orbital — `li-physics-*` + `sim.viz`. Tier-2 benches run **in** the engine viewport.

RFC: [sim-viz-scientific-rfc.md](specs/sim-viz-scientific-rfc.md)

---

## 12. AI drug design (PH-DRUG)

Roche **Lab-in-the-Loop**–class workflow: hypothesis → generate → **DFT/TDDFT** (`li-chem`) → lab ingest → retrain. **`studio.adaptive`** panels by stage/role.

RFC: [drug-design-lab-loop-rfc.md](specs/drug-design-lab-loop-rfc.md)

---

## 13. Quantum chemistry — `li-chem` (PH-QM)

**Separate from** gameplay `physics.chem`.

```li
import chem.dft

def benzene_energy() -> float
  requires geom_valid(benzene_ring())
=
  chem.dft.run(method = "rks", basis = "def2-svp", geometry = benzene_ring(), backend = "auto").total_energy
```

| Module | Methods |
|--------|---------|
| `chem.abinitio` | HF, MP2 (staged) |
| `chem.dft` | Ground-state DFT |
| `chem.tddft` | Excited states |
| `chem.ml` | AI surrogates (PH-ML) |

Backends: native Li → GPU/LKIR → trusted ORCA/Psi4. RFC: [li-chem-qm-rfc.md](specs/li-chem-qm-rfc.md)

---

## 14. Voxels (PH-VOXEL)

Unified **`VoxelGrid`** for games (blocks), AM (powder bed), engineering grids, scientific fields, QM density.

RFC: [voxel-engine-rfc.md](specs/voxel-engine-rfc.md)

---

## 15. Publication export (PH-PUB)

Paper-ready **figures** (SVG, PDF, 300+ dpi PNG) and **data** (HDF5, CSV, VTK) + **reproducibility bundle**.

```li
# conceptual API
# studio.publish.figure(path = "figures/fig1.svg", template = "nature_single_column")
# studio.publish.bundle(path = "publish.zip")
```

RFC: [publication-export-rfc.md](specs/publication-export-rfc.md)

---

## 16. ML / RL async (PH-ML)

Four parallelism axes: sample (env pools), host prefetch, GPU streams, cluster (Triton-distributed–class). Default `ml.runtime.mode = "async_parallel"`.

RFC: [ml-async-parallel-rfc.md](specs/ml-async-parallel-rfc.md)

---

## 17. PH-UX — killer UI/UX

| Target | Metric |
|--------|--------|
| Viewport | ≥60 fps |
| Panel switch | &lt;100 ms |
| AM export | ≤3 clicks |
| a11y | WCAG 2.2 AA chrome |
| Design system | `studio.design` tokens |

RFC: [studio-ux-design-system-rfc.md](specs/studio-ux-design-system-rfc.md)

---

## 18. PH-AGENT — Cursor SDK + MCP

| Tool (examples) | Action |
|-----------------|--------|
| `world_scaffold` | New project from template |
| `sim_set_profile` | Switch engine profile |
| `lic_check` / `lic_build` | Proof gate |
| `am_export_print` | Export to printer |
| `chem_dft_run` | Queue QM job |
| `publish_bundle` | Repro archive |
| `studio_adaptive_layout` | Drug/role layouts |

---

## 19. PH-HW — `lig`, multi-vendor GPU, LKIR

| Backend | Use |
|---------|-----|
| `lig.present` / wgpu | Viewport (all vendors) |
| CUDA | NVIDIA (`lig.device`) |
| **ROCm/HIP** | AMD (peer, not afterthought) |
| Metal | Apple Silicon / macOS |
| LKIR | Li Kernel IR under `packages/lig/lkir/` → CUDA/HIP/Metal/SPIR-V |

**Import:** `import lig` (replaces `import gpu` / package `li-gpu`).

**Portability:** `lic build --target <triple>` — RFC: [portable-targets-rfc.md](specs/portable-targets-rfc.md), [lig-rfc.md](specs/lig-rfc.md)

---

## 20. PH-COMPLY — critical packages

| Tier | Packages |
|------|------------|
| **CRITICAL** | `lic`, `li-chem`, `li-sim-drug-design`, `li-sim-additive`, `studio.publish` |
| **IMPORTANT** | `li-ml`, `li-sim`, `lig` |

CRITICAL: traceability `PKG-*`, SBOM, export audit log. RFC: [critical-package-compliance-rfc.md](specs/critical-package-compliance-rfc.md)

---

## 21. Technology choices

| Concern | Choice |
|---------|--------|
| Viewport | wgpu / Vulkan / Metal |
| UI | Extend `li-ui` (immediate mode, agent-readable) |
| GPU ML | LKIR + optional Triton; ROCm + CUDA |
| Agents | MCP + `@cursor/sdk` |
| Syntax | **`def`** + contracts |
| Assets | glTF primary |

---

## 22. Success metrics (rollup)

| Area | Target |
|------|--------|
| Game | Text → playable &lt;5 min (local 8B) |
| UX | SUS ≥75; 60 fps viewport |
| Sim | One `world.li` across game + sim profiles |
| AM | Sim pass → export 3MF/G-code |
| Drug | Lab-in-the-loop demo + adaptive stages |
| QM | DFT smoke + optional Psi4 parity bench |
| Publish | SVG + HDF5 + repro bundle |
| Agents | 70% fix-rate on curated `lic check` prompts |
| Compliance | CRITICAL packages SBOM in CI |

---

## 23. Immediate actions

1. Land RFC stubs under [specs/](specs/) (this execution).  
2. Tracker: [PH-world-studio-program.md](PH-world-studio-program.md).  
3. Update [li-world-studio-vision.mdc](../../.cursor/rules/li-world-studio-vision.mdc).  
4. Scaffold `li-studio` after GD-1 approval (separate PR).  
5. **Do not** use `li-demo` for studio features.

---

## 24. RFC index

| RFC | Track |
|-----|-------|
| [li-engine-unified-sim-rfc.md](specs/li-engine-unified-sim-rfc.md) | PH-SIM |
| [studio-cursor-sdk-rfc.md](specs/studio-cursor-sdk-rfc.md) | PH-AGENT |
| [studio-ux-design-system-rfc.md](specs/studio-ux-design-system-rfc.md) | PH-UX |
| [li-sim-additive-rfc.md](specs/li-sim-additive-rfc.md) | PH-AM |
| [li-sim-robotics-rfc.md](specs/li-sim-robotics-rfc.md) | PH-ROBO |
| [sim-viz-scientific-rfc.md](specs/sim-viz-scientific-rfc.md) | PH-SCI |
| [drug-design-lab-loop-rfc.md](specs/drug-design-lab-loop-rfc.md) | PH-DRUG |
| [li-chem-qm-rfc.md](specs/li-chem-qm-rfc.md) | PH-QM |
| [voxel-engine-rfc.md](specs/voxel-engine-rfc.md) | PH-VOXEL |
| [publication-export-rfc.md](specs/publication-export-rfc.md) | PH-PUB |
| [ml-async-parallel-rfc.md](specs/ml-async-parallel-rfc.md) | PH-ML |
| [portable-targets-rfc.md](specs/portable-targets-rfc.md) | PH-PORT |
| [lig-rfc.md](specs/lig-rfc.md) | PH-HW |
| [critical-package-compliance-rfc.md](specs/critical-package-compliance-rfc.md) | PH-COMPLY |

**Maintainers:** Quarterly SOTA review; keep in sync with Cursor plan artifact `world_studio_amd_port_be6fdf4f.plan.md`.
