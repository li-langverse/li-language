# li-sim-automotive

Domain profile stub for **sim_automotive** (World Studio profile id **3**, sim contract **3**).

Driving, sensors, and maps land here in PH-SIM follow-ups; wires `sim.automotive` against `li-sim` contracts.

## Build

```bash
lic build src/lib.li -o li-sim-automotive
lic check packages/li-sim-automotive/li-tests/smoke/builds.li
```

## Traceability

| ID | Link |
|----|------|
| Package | `PKG-li-sim-automotive` |
| Studio profile | `sim_automotive` → `li_sim_profile_from_studio_id(3)` |
| Org repo | https://github.com/li-langverse/li-sim-automotive |

## License

Apache-2.0 OR MIT
