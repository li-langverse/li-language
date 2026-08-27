---
name: studio-ui-ux-rubric
description: >-
  Score Li Studio against UX-01…14 and PH-UX gates using the competitive
  rubric and ux-harness. Use every studio_ui_ux_builder iteration.
---

# Studio UI/UX rubric

## Canonical docs

- Rubric: `lic/docs/game-dev/competitive-intel/ui-ux-by-dimension.md`
- Bench registry: `lic/benchmarks/competitive/studio-ui.toml`
- SOTA library: `ux-harness/sota/manifest.yaml`

## Scoring table (required in PR)

For each **UX-01 … UX-14**: score **0–3** and one-line rationale.

| Score | Meaning |
|------:|---------|
| 0 | Missing or broken |
| 1 | Stub / misleading |
| 2 | Usable, gaps vs SOTA |
| 3 | Meets PH-UX / competitive bar |

## PH-UX gates (pass/fail + evidence)

| Gate | Target | Evidence |
|------|--------|----------|
| viewport_fps | 60 | bench JSON or HUD |
| panel_switch_ms | 100 | timing hook or measured |
| studio_load_ms | 2000 | `latest-bench.json` |
| md particles | tier table | bench particle_tiers |
| memory | budget | profile-animate-memory lines |

## ux-harness

```bash
python3 ux-harness/run_audit.py --target world-studio-demo --mode ux
```

Journey IDs: `studio_workspace`, `command_palette`, `vertical_profile`.

## Upstream references

See `docs/agent-skills/awesome-ui-ux-sources.md` for VoltAgent awesome-list mappings.
