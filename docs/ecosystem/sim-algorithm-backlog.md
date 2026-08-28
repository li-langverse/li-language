# Simulation algorithm backlog (agent todos)

**Status:** Active  
**Registry:** `benchmarks/competitive/algo_registry.json`

---

todos:

- id: sim-p0-md-lj-li-parity
  content: "Tier-2 Li md_lennard_jones checksum parity vs native (fix runtime sink / driver)"
  status: pending

- id: sim-p0-heat-li-smoke
  content: "Pure-Li heat_equation_2d smoke with harness verify row"
  status: pending

- id: sim-p1-num-dot-axpy
  content: "Implement algo_id=1 num_dot_axpy in li-math-numerics + bench-package gate"
  status: pending

- id: sim-p1-md-neighbor-cell
  content: "Implement algo_id=105 md_neighbor_cell_list stub→smoke in li-physics-particles"
  status: pending

- id: sim-p2-qm-dft-scf
  content: "Implement algo_id=418 qm_dft_scf_energy minimal SCF stub with summary metrics"
  status: pending

---

## Agent instructions

- One todo per loop iteration (`sim-plan-loop.py`).
- After each slice: `./scripts/sim-plan-gates.sh` (uses `bench-package.sh`, not full tier-12).
- Update `implemented_smoke` in registry when a composable or tier-2 row exists.
