# Simulation output contract (machine-readable)

**Status:** Active (2026-05-24)  
**Parent:** [sim-packages-algorithm-plan.md](sim-packages-algorithm-plan.md) §6 · AL-15  
**Harness today:** `benchmarks/harness/verify.py`, `bench.py`, `md_external_oracle.py`

All `sim.*` runs and tier-2 physics benches should emit **structured, parser-friendly** artifacts. Humans read plots and Studio UI; agents, CI, and dashboards read **JSON** (and CSV where ingest already exists).

---

## Format choice: JSON vs JSONC vs others

| Format | Use for | CI parse? | Notes |
|--------|---------|-----------|-------|
| **JSON** (`.summary.json`, pretty) | Tier **S** summaries, oracle manifests, API responses | **Yes** — default for `verify.py` | Strict RFC 8259; `jq`, Python `json`, agents |
| **JSON minified** (`.summary.min.json`) | Same schema, fewer bytes / tokens | **Yes** | `json.dumps(..., separators=(",", ":"))`; default for Li `sim-write-summary.py` |
| **YAML** (`.summary.yaml`) | Human skim, compact agents that prefer YAML | **Yes** (optional) | Same keys as JSON; not the primary CI default |
| **JSONC** (`.jsonc`) | Tier **R** repro bundles only (hand-edited) | After strip-comments | Comments for humans; **never** the only gate artifact |
| **CSV** | Perf sweep rows (`latest.csv`) | Yes | Keep for [benchmarks dashboard](https://li-langverse.github.io/benchmarks/); one row per lang/run |
| **TOML** | **Inputs** (`params.toml`) | N/A | Not simulation output |
| **NDJSON** | Optional Tier **D** frame stream | Optional | One JSON object per line; stream-friendly |
| **VTK / HDF5 / NPZ** | Tier **F** arrays | Via libs | Binary fields; JSON **sidecar** for metadata |

**Recommendation:** Prefer **plain JSON** for anything CI or agents must consume. Use **JSONC** only inside `publish.zip`-style repro bundles where a scientist may annotate `// basis: def2-svp`. Provide `scripts/validate-sim-summary.sh` that parses JSON (and optionally JSONC via `strip-json-comments` or `jq` with a preprocess step).

Use **YAML summaries only when explicitly requested** (`--summary-format yaml` / `LI_SIM_SUMMARY_FORMAT=yaml`). CI gates should prefer **JSON** or **minified JSON**. Do **not** rely on stdout prose or multi‑MB `.traj` text for gates.

---

## Schema: `li_sim_summary_v1`

One file per run (or per bench×lang verify):

**Paths (same schema, different serializers):**

| Format | Path pattern |
|--------|----------------|
| Pretty JSON | `benchmarks/results/<benchmark_id>/<lang>.summary.json` |
| Minified JSON | `benchmarks/results/<benchmark_id>/<lang>.summary.min.json` |
| YAML | `benchmarks/results/<benchmark_id>/<lang>.summary.yaml` |
| Li composable runs | `benchmarks/results/li_runs/<algo_name>.li.summary.*` |

**Also:** aggregate `benchmarks/results/<benchmark_id>/summary.json` when a single canonical run exists.

```json
{
  "schema": "li_sim_summary_v1",
  "benchmark": "md_lennard_jones",
  "vertical_id": "md_lennard_jones",
  "workload_class": "stub",
  "lang": "cpp",
  "variant": "reference_native",
  "ok": true,
  "git_sha": "64e36ec",
  "cpu_model": "x86_64",
  "flags": "-O3 -march=native",
  "params_digest": "sha256:…",
  "metrics": {
    "energy_drift_rel": 1.2e-4,
    "checksum": "…",
    "wall_time_s": null
  },
  "invariants": {
    "energy_drift_ok": true
  },
  "artifacts": {
    "params": "benchmarks/tier2_physics/md_lennard_jones/params.toml",
    "tier_f": null,
    "tier_d": null
  },
  "updated": "2026-05-24T12:00:00Z"
}
```

### Required keys (all domains)

| Key | Type | Purpose |
|-----|------|---------|
| `schema` | string | Always `"li_sim_summary_v1"` |
| `benchmark` or `vertical_id` | string | Registry id |
| `ok` | bool | Single gate for scripts |
| `metrics` | object | Domain scalars (see below) |
| `updated` | string | ISO-8601 UTC |

### `metrics` by domain (extend, don’t fork schema)

| Domain | Keys in `metrics` |
|--------|-------------------|
| MD | `energy_drift_rel`, `checksum`, `N`, `dt`, `steps` |
| PDE/heat | `checksum` or `l2_error`, `cfl_max`, `nx`, `ny` |
| QM | `total_energy_hartree`, `converged`, `scf_iterations`, `method`, `basis` |
| AM | `layer_count`, `max_delta_T`, `gcode_bytes` |
| Rigid | `constraint_residual_max`, `steps` |

Perf timing may duplicate `latest.csv` but belongs in `metrics.wall_time_s` when written from the same harness invocation.

---

## Tier R repro bundle layout

```
publish/
  manifest.json          # strict JSON index (machine)
  manifest.notes.jsonc   # optional human comments (not CI-gated)
  params.toml
  summaries/
    cpp.summary.json
    li.summary.json
  fields/                # optional Tier F
    heat_000.vtk
```

`manifest.json` lists paths + digests (SHA-256). Agents validate the bundle without parsing JSONC.

---

## Adjustable detail (parameters + CLI + env)

| Level | Value | JSON summary | Extra artifacts (paths in `artifacts`) |
|-------|-------|--------------|----------------------------------------|
| `summary` | `0` | **Yes** — always | none |
| `fields` | `1` | Yes | `tier_f/` (VTK/HDF5 when implemented) |
| `debug` | `2` | Yes | `tier_d/` (NDJSON / `.traj` on demand) |
| `repro` | `3` | Yes | `publish/` bundle index |

### CLI (harness / native benches)

```bash
# Tier-2 smokes → machine-readable summaries
python3 benchmarks/harness/verify.py --write-summary --output-detail summary
python3 benchmarks/harness/verify.py --write-summary --summary-format json_min
python3 benchmarks/harness/verify.py --write-summary --summary-format yaml --output-detail debug

./li-tests/tooling/sim_li_run_summary.sh
./scripts/validate-sim-summary.sh
```

| Flag / env | Purpose |
|------------|---------|
| `--write-summary` | Write tier-2 summary under `benchmarks/results/<bench>/` |
| `--summary-format {json,json_min,yaml}` | Serializer (default `json` for verify, `json_min` for Li helper) |
| `LI_SIM_SUMMARY_FORMAT` | Same as `--summary-format` |
| `--output-detail {summary,fields,debug,repro}` | Sets `output_detail` + `artifacts.*` hints in JSON |
| `LI_SIM_OUTPUT_DETAIL` | Same as `--output-detail` for `verify.py` / `bench.py` |
| `scripts/sim-write-summary.py` | Li run → same `build_summary_from_li_run()` as verify |
| `LI_SIM_ALGO_ID`, `LI_SIM_OK`, `LI_SIM_CHECKSUM`, `LI_SIM_VERTICAL_ID` | Env inputs for `sim-write-summary.py --from-env` |
| `output_detail_summary()` … `repro()` | Li API: pass int to `run_simulation` / `run_algo` (0–3) |
| `LI_MD_DUMP_TRAJ=1` | Tier D only — existing trajectory export |

Higher detail **never** removes Tier S JSON; it only adds optional paths.

---

## Callable from other Li programs

Packages: `import sim` (types + ids) · `import sim.scientific` (MD/PDE/rigid smokes today).

```li
import sim
import sim.scientific

def main() -> int
=
  var r: SimRunResult = run_simulation(vertical_md_lennard_jones(), output_detail_summary())
  if run_result_ok(r) != 1:
    return 1
  var a: SimRunResult = run_algo(algo_pde_heat_explicit_2d(), output_detail_fields())
  return 1 - run_result_ok(a)
```

(`lic` today: import both packages; use **unqualified** types and `run_*` from their defining package — no `sim.scientific.run_simulation` qualified calls yet.)

| API | Role |
|-----|------|
| `sim.output_spec_default()` / `output_spec_from_detail(n)` | Parameterize verbosity |
| `run_simulation(vertical_id, detail)` | One entry per `verticals.toml` row; `detail` = `output_detail_*()` |
| `run_algo(algo_id, detail)` | One entry per `algo_*` id in [sim-packages-algorithm-plan.md](sim-packages-algorithm-plan.md) |
| `output_spec_from_detail(n)` | Build `SimOutputSpec` when you need the struct (moves — do not reuse) |
| `sim.run_result_ok(r)` | In-process gate (stdout not required) |

Future: `sim.drug_design`, `sim.additive`, `chem` export the same `SimRunResult` + write JSON via harness or `sim.write_summary` when path strings land in `lic`.

Composable smoke: `li-tests/composable/import_sim_scientific_run.li`.

---

## Harness integration

1. **Done:** `verify.py --write-summary` → `li_sim_summary_v1` per tier-2 smoke (`json` / `json_min` / `yaml`).  
2. **Done:** `sim_summary.py` + `sim-write-summary.py` + `sim_li_run_summary.sh` for Li runs.  
3. **Done:** `benchmarks/competitive/algo_registry.json` (126 ids); `run_algo` dispatches registry stubs.  
4. **Open:** `bench.py` merge `wall_time_s` into summary.  
5. **Done:** `./scripts/validate-sim-summary.sh` — schema check (JSON + min JSON + YAML).  
4. **Open:** `SimResult` JSON serialize from Li when string I/O exists in `lic`.

---

## Anti-patterns

| Avoid | Instead |
|-------|---------|
| Parsing `%.17g` trajectory walls of text in CI | `metrics.checksum` / `energy_drift_rel` in JSON |
| One giant JSON array of all frames | NDJSON Tier D or VTK Tier F |
| Markdown tables as run output | `*.summary.json` |
| JSONC-only oracle gate | strict `manifest.json` + optional `.jsonc` notes |
