#!/usr/bin/env python3
"""Generate X-ready benchmark plots from results/latest.csv."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

import pandas as pd

HARNESS = Path(__file__).resolve().parent
REPO = HARNESS.parent.parent
RESULTS = REPO / "benchmarks" / "results"
SHARE = RESULTS / "share"

TIER1_MICRO = {
    "simd_dot",
    "matmul_naive",
    "matmul_blocked",
    "reduce_sum",
    "horner_pure_li",
}
TIER2_PHYSICS = {
    "md_lennard_jones",
    "three_body",
    "nbody_gravity",
    "harmonic_oscillator_chain",
    "wave_equation_1d",
    "heat_equation_2d",
    "double_pendulum",
}

sys.path.insert(0, str(HARNESS))
from plot_theme import (  # noqa: E402
    LANG_COLORS,
    MUTED,
    PRIMARY,
    TEXT,
    WARN,
    brand_figure,
    save_share,
)


def git_sha() -> str:
    try:
        return (
            subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=REPO)
            .decode()
            .strip()
        )
    except Exception:
        return "unknown"


def load_csv(path: Path) -> pd.DataFrame:
    if not path.exists():
        raise FileNotFoundError(f"missing {path}")
    return pd.read_csv(path)


def load_verify_df() -> pd.DataFrame | None:
    verify_path = RESULTS / "verify.csv"
    if not verify_path.exists():
        return None
    return pd.read_csv(verify_path)


def plot_speed_bars(df: pd.DataFrame, tier: str, out: Path) -> None:
    import matplotlib.pyplot as plt
    import numpy as np

    if tier == "tier2":
        sub = df[df["benchmark"].isin(TIER2_PHYSICS)]
    elif tier == "tier1":
        sub = df[df["benchmark"].isin(TIER1_MICRO)]
    elif tier != "all":
        sub = df[df["benchmark"].str.contains(tier, case=False, na=False)]
    else:
        sub = df
    if sub.empty:
        if tier == "tier2":
            sub = df[df["benchmark"].isin(TIER2_PHYSICS)]
        elif tier == "tier1":
            sub = df[df["benchmark"].isin(TIER1_MICRO)]
        else:
            sub = df
    time_df = sub[sub["metric"].isin(["wall_time", "time", "latency"]) | sub["unit"].str.contains("s", na=False)]
    if time_df.empty:
        time_df = sub

    benchmarks = sorted(time_df["benchmark"].unique())
    langs = sorted(time_df["lang"].unique())
    x = np.arange(len(benchmarks))
    width = 0.8 / max(len(langs), 1)

    fig, ax = brand_figure(
        "Benchmark throughput",
        f"Lower is better · tier={tier} · sha={git_sha()}",
    )
    for i, lang in enumerate(langs):
        vals = []
        for b in benchmarks:
            row = time_df[(time_df["benchmark"] == b) & (time_df["lang"] == lang)]
            vals.append(float(row["value"].iloc[0]) if len(row) else 0)
        offset = (i - len(langs) / 2 + 0.5) * width
        ax.bar(
            x + offset,
            vals,
            width * 0.92,
            label=lang,
            color=LANG_COLORS.get(lang, PRIMARY),
            edgecolor="none",
        )
        for xi, v in zip(x + offset, vals):
            if v > 0:
                ax.text(xi, v, f"{v:.2g}", ha="center", va="bottom", fontsize=8, color=MUTED)

    ax.set_xticks(x)
    ax.set_xticklabels(benchmarks, rotation=20, ha="right")
    ax.set_ylabel(time_df["unit"].iloc[0] if len(time_df) else "value")
    ax.legend(loc="upper right", framealpha=0.9)
    ax.grid(axis="y", linestyle="--", linewidth=0.6)
    plt.tight_layout()
    save_share(fig, out)
    plt.close(fig)


def plot_speedup_vs_cpp(df: pd.DataFrame, out: Path) -> None:
    import matplotlib.pyplot as plt
    import numpy as np

    physics = df[df["benchmark"].isin(TIER2_PHYSICS)]
    li = physics[physics["lang"] == "li"].set_index("benchmark")["value"]
    cpp = physics[physics["lang"] == "cpp"].set_index("benchmark")["value"]
    common = li.index.intersection(cpp.index)
    if len(common) == 0:
        return
    speedup = (cpp[common] / li[common]).sort_values()

    fig, ax = brand_figure("Li vs C++ speedup", "Ratio > 1 means Li is faster")
    colors = [PRIMARY if v >= 1 else "#f85149" for v in speedup.values]
    y = np.arange(len(speedup))
    ax.barh(y, speedup.values, color=colors, edgecolor="none", height=0.65)
    ax.axvline(1.0, color=MUTED, linestyle="--", linewidth=1.2, label="parity")
    ax.set_yticks(y)
    ax.set_yticklabels(speedup.index)
    ax.set_xlabel("× speedup (cpp_time / li_time)")
    ax.legend(loc="lower right")
    ax.grid(axis="x", linestyle="--", linewidth=0.6)
    plt.tight_layout()
    save_share(fig, out)
    plt.close(fig)


def plot_correctness_grid(df: pd.DataFrame, out: Path) -> None:
    import matplotlib.pyplot as plt
    import numpy as np

    if "passed" not in df.columns:
        return
    benches = df["benchmark"].unique()
    langs = sorted(df["lang"].unique())
    grid = np.zeros((len(benches), len(langs)))
    for i, b in enumerate(benches):
        for j, lang in enumerate(langs):
            row = df[(df["benchmark"] == b) & (df["lang"] == lang)]
            grid[i, j] = 1.0 if len(row) and bool(row["passed"].iloc[0]) else 0.0

    fig, ax = brand_figure("Correctness gate", "Green = verify.py pass")
    im = ax.imshow(grid, aspect="auto", cmap="RdYlGn", vmin=0, vmax=1)
    ax.set_xticks(range(len(langs)))
    ax.set_xticklabels(langs)
    ax.set_yticks(range(len(benches)))
    ax.set_yticklabels(benches)
    for i in range(len(benches)):
        for j in range(len(langs)):
            sym = "✓" if grid[i, j] > 0.5 else "✗"
            ax.text(j, i, sym, ha="center", va="center", color=TEXT, fontsize=14)
    fig.colorbar(im, ax=ax, fraction=0.02, pad=0.02)
    plt.tight_layout()
    save_share(fig, out)
    plt.close(fig)


def load_energy_traces(trace_dir: Path) -> dict[str, pd.DataFrame]:
    traces: dict[str, pd.DataFrame] = {}
    for lang in ("cpp", "rust", "julia"):
        path = trace_dir / f"energy_{lang}.csv"
        if path.exists():
            traces[lang] = pd.read_csv(path)
    return traces


def _energy_drift_pct(df: pd.DataFrame) -> float:
    e0 = float(df["etotal"].iloc[0])
    e1 = float(df["etotal"].iloc[-1])
    denom = max(abs(e0), abs(e1), 1e-12)
    return 100.0 * abs(e1 - e0) / denom


def plot_md_energy_by_lang(trace_dir: Path, out: Path) -> None:
    import matplotlib.pyplot as plt

    from plot_theme import FAIL, PANEL, apply_theme

    traces = load_energy_traces(trace_dir)
    if not traces:
        print(f"skip energy-by-lang plot: no traces in {trace_dir}", file=sys.stderr)
        return

    apply_theme()
    langs = [lang for lang in ("cpp", "rust", "julia") if lang in traces]
    fig, axes = plt.subplots(len(langs), 1, figsize=(16, 9), sharex=True)
    if len(langs) == 1:
        axes = [axes]

    fig.patch.set_facecolor(PANEL)
    fig.suptitle(
        "md_lennard_jones energy traces",
        fontsize=20,
        fontweight="bold",
        color=TEXT,
        x=0.08,
        ha="left",
        y=0.98,
    )
    fig.text(
        0.08,
        0.94,
        f"N=256 · dt=0.004 · 10k steps · sampled every 25 · sha={git_sha()}",
        fontsize=12,
        color=MUTED,
    )

    pe_color = LANG_COLORS.get("cpp", "#f0883e")
    ke_color = PRIMARY
    total_color = "#e6edf3"

    for ax, lang in zip(axes, langs):
        df = traces[lang]
        time_fs = df["step"] * 0.004
        drift = _energy_drift_pct(df)
        ax.plot(time_fs, df["pe"], label="E_pot", color=pe_color, linewidth=1.6)
        ax.plot(time_fs, df["ke"], label="E_kin", color=ke_color, linewidth=1.6)
        ax.plot(time_fs, df["etotal"], label="E_total", color=total_color, linewidth=2.0, alpha=0.9)
        ax.set_ylabel("energy (LJ)")
        ax.set_title(f"{lang} · |ΔE|/E = {drift:.2f}%", loc="left", color=TEXT, fontsize=14)
        ax.grid(axis="y", linestyle="--", linewidth=0.6, alpha=0.5)
        ax.legend(loc="upper right", framealpha=0.9)
        if drift > 0.1:
            ax.text(
                0.02,
                0.05,
                "unstable / not conserving",
                transform=ax.transAxes,
                color=FAIL,
                fontsize=11,
                fontweight="bold",
            )

    axes[-1].set_xlabel("time (LJ units)")
    fig.text(0.96, 0.03, "Li · li-language", ha="right", fontsize=10, color=MUTED)
    plt.tight_layout(rect=(0, 0, 1, 0.92))
    save_share(fig, out)
    plt.close(fig)


STABILITY_STRICT = {"harmonic_energy", "momentum_drift"}
STABILITY_ADVISORY = {"nve_energy_msd", "timestep_halving_ratio"}


def plot_stability_by_lang(stability_csv: Path, out: Path) -> None:
    import matplotlib.pyplot as plt
    import numpy as np

    from plot_theme import FAIL, PASS, PANEL, apply_theme

    if not stability_csv.exists():
        print(f"skip stability plot: missing {stability_csv}", file=sys.stderr)
        return

    df = pd.read_csv(stability_csv)
    if "lang" not in df.columns:
        print(f"skip stability plot: missing lang column in {stability_csv}", file=sys.stderr)
        return

    apply_theme()
    tests = list(dict.fromkeys(df["test"].tolist()))
    langs = [lang for lang in ("cpp", "rust", "julia", "li") if lang in set(df["lang"])]
    if not tests or not langs:
        return

    width = 0.18
    offsets = np.linspace(-(len(langs) - 1) / 2, (len(langs) - 1) / 2, len(langs)) * width

    fig, axes = plt.subplots(1, 2, figsize=(16, 9))
    fig.patch.set_facecolor(PANEL)
    fig.suptitle(
        "md_lennard_jones stability by language",
        fontsize=20,
        fontweight="bold",
        color=TEXT,
        x=0.08,
        ha="left",
        y=0.98,
    )
    fig.text(
        0.08,
        0.94,
        f"shared C stress kernel · cpp/rust/julia identical · Li via lic+md_stress.c · sha={git_sha()}",
        fontsize=12,
        color=MUTED,
    )

    strict_tests = [t for t in tests if t in STABILITY_STRICT]
    advisory_tests = [t for t in tests if t in STABILITY_ADVISORY]

    def draw_panel(ax, panel_tests: list[str], title: str) -> None:
        if not panel_tests:
            ax.set_visible(False)
            return
        px = np.arange(len(panel_tests))
        for off, lang in zip(offsets, langs):
            vals = []
            colors = []
            for test in panel_tests:
                sub = df[(df["lang"] == lang) & (df["test"] == test)]
                if sub.empty:
                    vals.append(np.nan)
                    colors.append(MUTED)
                    continue
                row = sub.iloc[0]
                vals.append(float(row["value"]))
                colors.append(PASS if bool(row["passed"]) else FAIL)
            bars = ax.bar(
                px + off,
                vals,
                width=width,
                label=lang,
                color=LANG_COLORS.get(lang, PRIMARY),
                alpha=0.85,
            )
            for bar, color in zip(bars, colors):
                bar.set_edgecolor(color)
                bar.set_linewidth(2.0)
            for test in panel_tests:
                sub = df[df["test"] == test]
                if sub.empty:
                    continue
                thr = float(sub.iloc[0]["threshold"])
                idx = panel_tests.index(test)
                ax.hlines(thr, idx - 0.4, idx + 0.4, colors=WARN, linestyles="--", linewidth=1.2)
        ax.set_yscale("log")
        ax.set_xticks(px)
        ax.set_xticklabels(panel_tests, rotation=15, ha="right")
        ax.set_title(title, loc="left", color=TEXT)
        ax.set_ylabel("metric value (log)")
        ax.grid(axis="y", linestyle="--", linewidth=0.6, alpha=0.5)
        ax.legend(loc="upper right", framealpha=0.9)

    draw_panel(axes[0], strict_tests, "Strict (Swope / momentum)")
    draw_panel(axes[1], advisory_tests, "Advisory (Allen–Tildesley)")
    fig.text(0.96, 0.03, "Li · li-language", ha="right", fontsize=10, color=MUTED)
    plt.tight_layout(rect=(0, 0, 1, 0.92))
    save_share(fig, out)
    plt.close(fig)


def plot_md_energy_overlay(trace_dir: Path, out: Path) -> None:
    import matplotlib.pyplot as plt

    traces = load_energy_traces(trace_dir)
    if len(traces) < 2:
        return

    fig, axes = plt.subplots(1, 3, figsize=(16, 9), sharex=True)
    panels = [("pe", "Potential energy"), ("ke", "Kinetic energy"), ("etotal", "Total energy")]

    for ax, (col, title) in zip(axes, panels):
        for lang, df in traces.items():
            time_fs = df["step"] * 0.004
            ax.plot(
                time_fs,
                df[col],
                label=lang,
                color=LANG_COLORS.get(lang, PRIMARY),
                linewidth=1.8 if col == "etotal" else 1.4,
                alpha=0.95,
            )
        ax.set_title(title, loc="left", color=TEXT)
        ax.set_xlabel("time (LJ units)")
        ax.set_ylabel("energy (LJ)")
        ax.grid(axis="y", linestyle="--", linewidth=0.6)
        ax.legend(loc="best", framealpha=0.9)

    fig.suptitle(
        "md_lennard_jones cross-language energy comparison",
        fontsize=20,
        fontweight="bold",
        color=TEXT,
        x=0.06,
        ha="left",
        y=0.98,
    )
    plt.tight_layout(rect=(0, 0, 1, 0.94))
    save_share(fig, out)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot benchmark CSV for X")
    parser.add_argument("--csv", type=Path, default=RESULTS / "latest.csv")
    parser.add_argument("--out", type=Path, default=SHARE)
    parser.add_argument("--tier", default="tier2")
    parser.add_argument(
        "--stability-csv",
        type=Path,
        default=RESULTS / "stability.csv",
        help="stability harness CSV (lang × test)",
    )
    parser.add_argument(
        "--energy-dir",
        type=Path,
        default=RESULTS / "md_lennard_jones",
        help="directory with energy_{lang}.csv traces",
    )
    args = parser.parse_args()

    df = load_csv(args.csv)
    args.out.mkdir(parents=True, exist_ok=True)

    plot_speed_bars(df, args.tier, args.out / f"bench_speed_{args.tier}.png")
    if args.tier == "tier2":
        plot_speed_bars(df, "tier1", args.out / "bench_speed_tier1.png")
    plot_speedup_vs_cpp(df, args.out / "speedup_vs_cpp.png")
    verify_df = load_verify_df()
    if verify_df is not None and "passed" in verify_df.columns:
        plot_correctness_grid(verify_df, args.out / "correctness_tier0.png")

    plot_stability_by_lang(args.stability_csv, args.out / "md_stability_by_lang.png")
    plot_md_energy_by_lang(args.energy_dir, args.out / "md_lennard_jones_energy_by_lang.png")
    plot_md_energy_overlay(args.energy_dir, args.out / "md_lennard_jones_energy_overlay.png")

    print(f"wrote plots to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
