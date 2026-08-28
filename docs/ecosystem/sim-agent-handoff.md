# Simulation algorithms — goal-directed agent handoff

**Status:** 2026-05-24  
**Bench scope:** `benchmarks/manifest.toml` · `./scripts/bench-package.sh`  
**Registry:** `benchmarks/competitive/algo_registry.json` (126 `algo_id`s)  
**Output contract:** [sim-output-contract.md](sim-output-contract.md)

---

## Is the goal-directed agent ready?

| Capability | Ready? | Notes |
|------------|--------|-------|
| Canonical `algo_id` registry | **Yes** | `algo_registry.json` + `scripts/build_algo_registry.py` |
| Li dispatch surface | **Yes** | `run_algo` / `run_simulation` in `sim.scientific` (smokes + registry stubs) |
| Machine-readable summaries | **Yes** | `sim_summary.py`, `sim-write-summary.py`, `sim_li_run_summary.sh` |
| **Modular bench/verify** | **Yes** | `bench-package.sh`, `bench_sim.py`, `bench_scope.py` — no full tier-12 on package edits |
| Real algorithm kernels | **No** | Most ids are `registry_stub`; only MD/heat/rigid smokes are substantive |
| Tier-2 Li parity gate | **No** | Native C++ verify passes; full Li MD checksum parity still blocked on runtime sink |
| Autonomous plan loop | **Partial** | `scripts/sim-plan-loop.py` + `sim-plan-gates.sh` (mirror httpd loop); needs `CURSOR_API_KEY` |

**Verdict:** The agent can **implement, bench, and verify incrementally** per package without rerunning all Li benchmarks. It should **not** claim production parity or full 126-kernel coverage until stubs are replaced and tier-2 Li verify is green.

---

## Agent workflow (one algo or one package)

1. **Pick** `algo_id` from `algo_registry.json` (or plan slice).
2. **Implement** in the owning package (`li-sim-scientific`, `li-physics-*`, …).
3. **Wire** `run_algo` branch (replace stub) and set `implemented_smoke = true` in registry when a real smoke exists.
4. **Gate** (package-scoped only):

```bash
export LIC=build/compiler/lic/lic
./scripts/bench-package.sh li-sim-scientific --write-summary
# or after local edits:
./scripts/bench-package.sh --changed --write-summary
./scripts/sim-plan-gates.sh   # full sim agent gate set
```

5. **Emit summary** for CI/agents:

```bash
LI_SIM_ALGO_ID=418 LI_SIM_OK=1 LI_SIM_CHECKSUM=0.42 LI_SIM_VERTICAL_ID=4 \
  python3 scripts/sim-write-summary.py --format json_min -o benchmarks/results/li_runs/qm_dft_scf_energy.li.summary.min.json
./scripts/validate-sim-summary.sh
```

6. **Optional timing** (only mapped benches):

```bash
./scripts/bench-package.sh li-physics-particles --timing --runs 3
```

---

## Plan loop (goal-directed, runs until done)

**Continuous runner** (commit + push each iteration):

```bash
export CURSOR_API_KEY=... GH_TOKEN=...
export LI_CURSOR_AGENTS_ROOT=../li-cursor-agents
./scripts/sim-plan-run-until-done.sh   # foreground
# or: nohup ./scripts/sim-plan-run-until-done.sh >> data/sim-plan-loop/runner.log 2>&1 &
```

**Daily report (08:00, `SIM_PLAN_TZ`, default Europe/Berlin):**

```bash
./scripts/sim-plan-install-cron.sh    # once
./scripts/sim-plan-daily-report.sh  # manual
```

Reports: `docs/reports/sim-plan/daily/YYYY-MM-DD.md`, live `docs/reports/sim-plan/STATUS.md`.

Each iteration gates: **validity** + **performance** (`bench-package --timing`) + **memory** (`sim-bench-memory.sh`) + **docs** (`sim-plan-iteration-report.py`).

Todos: [sim-algorithm-backlog.md](sim-algorithm-backlog.md); when empty, loop picks next `implemented_smoke: false` from the registry.

---

## Package → bench map (summary)

| Package | Scoped tier-2 benches | Composable |
|---------|----------------------|------------|
| `li-sim-scientific` | `md_lennard_jones`, `heat_equation_2d` | `import_sim_scientific_run.li` |
| `li-physics-particles` | `md_lennard_jones`, `nbody_gravity`, `three_body` | — |
| `li-math-numerics` | tier-1 micro five-pack | — |

Full table: `benchmarks/manifest.toml`.

---

## Blockers before “production sim” claims

1. Replace registry stubs with proved kernels (Wave A: 2e/2f VC gates per [algorithms-and-libraries-plan.md](algorithms-and-libraries-plan.md)).
2. Tier-2 Li vs native checksum parity for `md_lennard_jones`.
3. External oracle column (LAMMPS/GROMACS) for MD — competitive plan item.
