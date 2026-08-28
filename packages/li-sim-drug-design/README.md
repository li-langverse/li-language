# li-sim-drug-design

Domain profile stub for **sim_drug_design** (World Studio profile id **7**, sim contract **7**).

Lab-in-the-loop and adaptive gui hooks land here in PH-SIM follow-ups; wires `sim.drug_design` against `li-sim` contracts.

## Build

```bash
lic build src/lib.li -o li-sim-drug-design
lic check packages/li-sim-drug-design/li-tests/smoke/builds.li
```

## Traceability

| ID | Link |
|----|------|
| Package | `PKG-li-sim-drug-design` |
| Studio profile | `sim_drug_design` → `li_sim_profile_from_studio_id(7)` |
| Org repo | https://github.com/li-langverse/li-sim-drug-design |

## License

Apache-2.0 OR MIT
