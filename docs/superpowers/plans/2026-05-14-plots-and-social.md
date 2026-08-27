# Li — Shareable Plots & X Assets

> **For agentic workers:** Ship publication-quality PNGs from CI and local `./scripts/plot_shareables.sh`. Every benchmark run and test sweep must be plottable without hand-editing.

**Goal:** Top-notch, on-brand figures for X, blog posts, and README — generated from `latest.csv` and `li-tests` manifest results.

**Depends on:** `benchmarks/harness/bench.py`, `li-tests/run_all.sh`  
**Blocks:** Phase 5b exit gate (publishable benchmark story)

---

## Design standards (X / social)

| Rule | Value |
|------|-------|
| Aspect ratio | **16:9** (1600×900 px @ 1×; export **3200×1800** for retina) |
| Background | Dark `#0d1117` (GitHub-style) — reads well in X dark mode |
| Accent | `#58a6ff` (Li primary), `#3fb950` pass, `#f85149` fail |
| Typography | Sans: system / Inter fallback; mono for numbers |
| Branding | Title: **Li** (理); subtitle: benchmark or suite name; footer: `li-language · git sha` |
| No chartjunk | No 3D, no pie charts for perf; bar/line/heatmap only |
| Accessibility | Color + hatch/pattern; label bars directly when <8 series |
| Formats | `benchmarks/results/share/*.png` + optional `*.svg` for blog |

---

## Outputs (required)

| Asset | Source | Filename pattern |
|-------|--------|------------------|
| Cross-lang speed bar | `results/latest.csv` | `share/bench_speed_{tier}.png` |
| Li vs C++ speedup | same | `share/speedup_vs_cpp.png` |
| Correctness badge grid | verify results | `share/correctness_tier0.png` |
| Test suite matrix | `li-tests` manifest run | `share/test_suite_matrix.png` |
| Suite pass rate bars | same | `share/test_suite_pass_rate.png` |
| CI summary card | combined JSON | `share/ci_summary_card.png` |

---

## Repository layout

```
benchmarks/
  harness/
    bench.py           # run benchmarks → latest.csv
    plot.py            # CSV → share PNGs
    plot_theme.py      # Li dark theme, fonts, sizes
    requirements.txt   # matplotlib, pandas, pillow
  results/
    latest.csv
    share/             # generated PNGs (gitignored except .gitkeep)
li-tests/
  harness/
    plot_suites.py     # run_all → suite plots
scripts/
  plot_shareables.sh   # one command: tests + benches → share/
```

---

## CSV → plot contract

`plot.py` reads columns: `benchmark, lang, variant, threads, metric, value, unit, git_sha, cpu_model, flags`

**Charts:**
1. **Grouped bar** — x=benchmark, hue=lang, y=value (log scale for time metrics)
2. **Speedup** — li / cpp per benchmark, horizontal bars, reference line at 1.0×
3. **Scaling** — line chart threads vs throughput for OpenMP benches

---

## li-tests → plot contract

`plot_suites.py`:
1. Parses `manifest.toml`, runs each suite via `run_all.sh`
2. Builds matrix: rows=suites, cols=outcome types, cell=count pass/fail
3. Emits pass-rate bar chart and heatmap

---

## Phase alignment

| When | Plots |
|------|-------|
| **Now (5b skeleton)** | Test suite plots from live `lic`; sample bench CSV + demo speed chart |
| Phase 3 codegen | Real Tier 1 micro bars |
| Phase 5b Tier 2 | Physics speed + correctness grid for X thread |
| Post v1 | Animated GIF optional (out of scope v1) |

---

## Exit gate (Phase 5b plotting slice)

- [ ] `./scripts/plot_shareables.sh` exits 0 and writes ≥4 PNGs to `benchmarks/results/share/`
- [ ] Plots meet 16:9 + dark theme spec above
- [ ] README or docs link to `results/share/` for social assets
- [ ] `bench.py --tier 0` feeds correctness plot when Tier 0 data exists

---

## X posting checklist

1. Run `./scripts/plot_shareables.sh` on reference machine
2. Attach `share/bench_speed_tier2.png` or `share/test_suite_pass_rate.png`
3. Alt text: describe metric, langs, machine, N=…
4. Tweet copy: lead with **proved** (Lean) + number; link repo
