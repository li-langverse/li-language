# li-nanoreactor

Li package li-nanoreactor — an ab initio nanoreactor: a reactive MD box with
thermal cycling and piston confinement for automated reaction discovery, as a
standalone installable Li package.

## Modules (each unit-tested independently, then plugged together)

| Module | Contents | Unit test |
|--------|----------|-----------|
| A — box/piston + thermal cycle | `nano_phase_of`, `nano_piston_z`, `nano_box_volume`, `nano_target_temp` | `li-tests/unit/box_cycle.li` |
| B — pair potentials | LJ nonbonded + Morse reactive bond scalars | `li-tests/unit/potential.li` |
| C — integrator | velocity-Verlet advance, kinetic energy, thermostat rescale | `li-tests/unit/integrator.li` |
| D — reaction detection | squared-distance bonds, bond counts, event detection | `li-tests/unit/reaction.li` |
| F — sample systems | water dimer, methane, H2+O2 | `li-tests/e2e/*.li` |
| E — reactor driver | thermal cycling + piston + integrate + detect → `NanoRunResult` | `li-tests/e2e/*.li` |

Every module is exercised **by itself** (unit tests) and on common sample
systems (e2e tests) before the driver plugs them together.

## Run the example

```bash
lic build packages/li-nanoreactor/examples/nanoreactor_run.li \
    -o /tmp/nanoreactor --allow-open-vc --no-lean-verify
/tmp/nanoreactor
```

## Run the tests

```bash
lic build packages/li-nanoreactor/li-tests/unit/box_cycle.li  -o /tmp/t --allow-open-vc && /tmp/t   # etc.
```

## Traceability

| ID | Link |
|----|------|
| Package | `PKG-li-nanoreactor` |
| Org repo | https://github.com/li-langverse/li-nanoreactor |
| Governance | [Ecosystem governance](https://li-langverse.github.io/li-language/ecosystem/governance/) |

See `PUBLISH.md` and `docs/traceability.md`.

## License

Apache-2.0 OR MIT
