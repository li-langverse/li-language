# Li World Studio — per-vertical demo script (~75–90s)

**Output:** `docs/demo/media/studio-verticals-demo.mp4`  
**Capture:** `LIG_HOST_PRESENT=1 ./scripts/record-studio-verticals-demo.sh`  
**Honesty:** MP4 frames are **native** (`native_pixels=1` CPU present host per profile). Compose contracts verified via `lic check`. HTML reference mocks live under `deploy/studio-demo/archive/verticals-html-mocks/` — not used in the user video.

| Beat | Profile | Duration | Voiceover |
|------|---------|----------|-----------|
| 1 | `game` | 10s | Li World Studio opens on the **game** profile: dock, viewport grid, selection ring, and timeline playhead — the default authoring shell. |
| 2 | `sim_rl` | 10s | Switch to **RL training**: profile chip, env viewport metrics, and agent context labeled **training env** — async env pools hook to `sim.step` later. |
| 3 | `sim_automotive` | 10s | **Automotive** shows an honest placeholder: driving scene and sensor rig are **not loaded** yet; profile + inspector scaffold only. |
| 4 | `sim_robotics` | 10s | **Robotics** highlights inspector joint fields (θ, torque limits) while IK and cell layout remain compose stubs. |
| 5 | `sim_additive` | 10s | **Additive** calls out export stubs — **3MF** and **G-code** after `require_sim_pass` — no live printer path in this build. |
| 6 | `sim_scientific` | 10s | **Scientific** labels particle **display tier** and MD integrator copy; tier-2 physics oracles are separate bench work. |
| 7 | `sim_drug_design` | 10s | **Drug design** surfaces adaptive-stage hints for Lab-in-the-Loop; binding pose agent line is mock chrome only. |
| 8 | outro | 5s | Run `lic check` on `studio_vertical_profile_roundtrip.li`; native frames show profile-colored topbar chip (not full domain packs). |

**On-screen:** Viewport grid + selection rect + profile chip color/height per `studio_profile_paint_tag_h` (see `VERTICALS-RECORDING.md`).
