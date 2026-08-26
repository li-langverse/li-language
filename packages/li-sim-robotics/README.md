# li-sim-robotics

Domain profile stub for **sim_robotics** (World Studio profile id **4**, sim contract **4**).

Arms, mobile bases, and factory cells land here in PH-SIM follow-ups; wires `sim.robotics` against `li-sim` contracts.

## Build

```bash
lic build src/lib.li -o li-sim-robotics
lic check packages/li-sim-robotics/li-tests/smoke/builds.li
```

## Traceability

| ID | Link |
|----|------|
| Package | `PKG-li-sim-robotics` |
| Studio profile | `sim_robotics` → `li_sim_profile_from_studio_id(4)` |
| Org repo | https://github.com/li-langverse/li-sim-robotics |

## License

Apache-2.0 OR MIT
