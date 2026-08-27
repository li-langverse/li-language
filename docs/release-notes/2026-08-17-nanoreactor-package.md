# 2026-08-17 — `li-nanoreactor` package + float-array codegen fix

## `li-nanoreactor` — standalone installable package

An ab initio nanoreactor in pure Li: a reactive MD box with thermal cycling
and piston confinement for automated reaction discovery, built module-by-module
so every module is verified **by itself** before the driver plugs them together.

`packages/li-nanoreactor/` (`import nanoreactor`), deps `li-chem` + `li-sim`:

| Module | Contents |
|--------|----------|
| A — box/piston + thermal cycle | `nano_phase_of`, `nano_piston_z`, `nano_box_volume`, `nano_target_temp` (pure, strictly discharged) |
| B — pair potentials | LJ nonbonded + Morse reactive bond scalars (pure, strictly discharged) |
| C — integrator | velocity-Verlet advance, kinetic energy, thermostat rescale on fixed 8-atom arrays |
| D — reaction detection | squared-distance bonds, bond counts, event detection |
| F — sample systems | water dimer, methane, H2+O2 (common reaction-discovery samples) |
| E — reactor driver | thermal cycling + piston + integrate + detect → `NanoRunResult` (ok/steps/events/checksum/energy_drift/final_bonds/final_ke) |

Testing discipline (per the request: modules verified by themselves before
being plugged together):

- `li-tests/unit/box_cycle.li`, `potential.li`, `integrator.li`, `reaction.li` —
  each module exercised standalone.
- `li-tests/e2e/water_dimer.li`, `methane.li`, `h2_o2.li` — the full driver on
  each common sample system.
- `examples/nanoreactor_run.li` — runnable example (build with `lic build
  ... --allow-open-vc`, then execute).
- Registered in `packages/li.toml` workspace and monorepo `li-tests/manifest.toml`
  (7 new tests, suite `nanoreactor`).

## Compiler fix found while building it: float-array element arithmetic

The nanoreactor's hot path (`acc = acc + a[j] * b[j]`, `dot4(a, b)` over
`array[4, float]`) exposed a codegen bug: `is_float_expr` in
`compiler/mir/lower.cpp` handled literals, idents, fields, and binops — but
**not** `Index` reads. So `a[0] * b[0]` on float arrays lowered through
`BinOpInt` (`fptosi` truncation), and a user-defined float-returning call with
array params returned uninitialized stack memory (`ret double` of a never-stored
temp). The manifest suite only *builds* programs, so this was invisible.

Fixes (all `compiler/mir/lower.cpp`):

- `is_float_expr` now resolves `Index` reads: float arrays, object array fields,
  and nested matrix indices (`m[i][j]`).
- Float-array params are seeded so `a[0]` inside callees is typed float.
- New runtime gate `li-tests/tooling/check_float_array_codegen_runtime.sh`
  builds **and runs** a dot4 + loop-indexed accumulation program, asserting
  exact values (8.0); wired into `scripts/ci.sh`.

## Verification

- `li-nanoreactor`: 7/7 tests pass (build + runtime asserts); example runs
  (water dimer detects a reaction event; methane/H2+O2 complete cleanly).
- No regressions: `math_linalg` 26/26, `contracts_verify` 32/32 (incl. the 2D
  float matmul certs and negative E0304 guard), `typecheck` 8/8, `physics` 3/3,
  `smoke` 5/5, `parallel_codegen`, `lexer_parser` 7/7, `effects` 12/12,
  `encapsulation` 26/26, `runtime` 4/4, `borrow` 3/3, `codegen`, `stdlib_coverage`.
- Parity gates: prover `cpp=22 li=18 open=0`, lexer gate ok.
- New runtime gate: ok.
