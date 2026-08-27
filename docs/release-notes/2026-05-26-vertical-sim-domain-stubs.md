# Release notes: 2026-05-26 — vertical-sim-domain-stubs

**Status:** Ready for review  
**Repo:** li-langverse/lic  
**PR:** (branch `feat/vertical-gap-sim-packs`)  
**PH / REQ:** PH-SIM (vertical gap #3)  
**Author:** agent

---

## Summary (one sentence)

Adds four minimal `li-sim-*` domain profile packages with workspace registration and `lic check` package smokes aligned to `li-sim` studio contract ids.

## Agent continuation (required)

1. Read: `docs/game-dev/world-studio-vision.md` §5–7; `packages/li-sim/src/lib.li`.
2. Run: `./scripts/build.sh`; `lic check` on each `packages/li-sim-*/li-tests/smoke/builds.li` (absolute paths from repo root).
3. Then: domain physics per vertical — not this PR.
4. Blocked on: none.

## Changed (specific)

| Area | What | Evidence |
|------|------|----------|
| Workspace | four new `packages/li.toml` members | member list |
| `li-sim-automotive` | `sim.automotive`; contract **3**, studio **3** | `lic check` smoke |
| `li-sim-robotics` | `sim.robotics`; contract **4**, studio **4** | `lic check` smoke |
| `li-sim-additive` | `sim.additive`; contract **5**, studio **5** | `lic check` smoke |
| `li-sim-drug-design` | `sim.drug_design`; contract **7**, studio **7** | `lic check` smoke |

## Not changed (scope fence)

- `li-sim-scientific` MD/PDE/rigid runners — unchanged.
- Studio demos, export.print, CARLA — follow-ups.

## Breaking changes

None.

## Security

N/A.

## Performance

N/A.

## Downstream

N/A.
