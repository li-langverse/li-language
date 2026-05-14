#!/usr/bin/env python3
"""Plot li-tests suite results for X sharing."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LI_TESTS = REPO / "li-tests"
HARNESS = REPO / "benchmarks" / "harness"
SHARE = REPO / "benchmarks" / "results" / "share"

sys.path.insert(0, str(HARNESS))
from plot_theme import FAIL, PASS, PRIMARY, MUTED, TEXT, brand_figure, save_share  # noqa: E402


def run_all_suites() -> tuple[dict[str, dict[str, int]], int, int]:
    lic = REPO / "build" / "compiler" / "lic" / "lic"
    env = {**dict(__import__("os").environ), "LIC": str(lic)}
    proc = subprocess.run(
        [str(LI_TESTS / "run_all.sh"), "all"],
        cwd=LI_TESTS,
        env=env,
        capture_output=True,
        text=True,
        timeout=120,
    )
    suites: dict[str, dict[str, int]] = defaultdict(lambda: {"pass": 0, "fail": 0, "skip": 0})
    out = proc.stdout + proc.stderr
    for line in out.splitlines():
        m = re.match(r"^(PASS|FAIL|SKIP) (\S+) ", line)
        if not m:
            continue
        status, path = m.group(1).lower(), m.group(2)
        sname = path.split("/")[0] if "/" in path else "unknown"
        suites[sname][status] += 1
    total_pass = sum(v["pass"] for v in suites.values())
    total_fail = sum(v["fail"] for v in suites.values())
    return suites, total_pass, total_fail


def plot_pass_rate(suites: dict[str, dict[str, int]], out: Path) -> None:
    import matplotlib.pyplot as plt
    import numpy as np

    names = sorted(suites.keys())
    rates = []
    for n in names:
        p = suites[n]["pass"]
        f = suites[n]["fail"]
        rates.append(p / (p + f) if (p + f) else 0.0)

    fig, ax = brand_figure("Li test suite pass rate", "From li-tests/manifest.toml")
    y = np.arange(len(names))
    colors = [PASS if r >= 0.99 else FAIL if r < 0.5 else PRIMARY for r in rates]
    ax.barh(y, rates, color=colors, edgecolor="none", height=0.6)
    ax.set_xlim(0, 1.05)
    ax.set_xlabel("pass rate")
    ax.set_yticks(y)
    ax.set_yticklabels(names)
    for yi, r in zip(y, rates):
        ax.text(r + 0.02, yi, f"{r:.0%}", va="center", color=TEXT, fontsize=11)
    ax.grid(axis="x", linestyle="--", linewidth=0.6)
    plt.tight_layout()
    save_share(fig, out)
    plt.close(fig)


def plot_matrix(suites: dict[str, dict[str, int]], out: Path) -> None:
    import matplotlib.pyplot as plt
    import numpy as np

    names = sorted(suites.keys())
    cols = ["pass", "fail", "skip"]
    grid = np.array([[suites[n][c] for c in cols] for n in names], dtype=float)

    fig, ax = brand_figure("Test suite matrix", "Rows = suite · Columns = pass / fail / skip")
    im = ax.imshow(grid, aspect="auto", cmap="Blues")
    ax.set_xticks(range(len(cols)))
    ax.set_xticklabels(cols)
    ax.set_yticks(range(len(names)))
    ax.set_yticklabels(names)
    for i in range(len(names)):
        for j in range(len(cols)):
            v = int(grid[i, j])
            if v:
                ax.text(j, i, str(v), ha="center", va="center", color=TEXT, fontsize=12)
    fig.colorbar(im, ax=ax, fraction=0.02, pad=0.02)
    plt.tight_layout()
    save_share(fig, out)
    plt.close(fig)


def plot_ci_card(total_pass: int, total_fail: int, out: Path) -> None:
    import matplotlib.pyplot as plt

    fig, ax = brand_figure("Li CI snapshot", "Compiler + conformance suites")
    ax.axis("off")
    total = total_pass + total_fail
    rate = total_pass / total if total else 0
    ax.text(0.5, 0.62, f"{rate:.0%}", ha="center", va="center", fontsize=72, color=PASS if rate >= 0.9 else FAIL, transform=ax.transAxes)
    ax.text(0.5, 0.38, f"{total_pass} passed · {total_fail} failed", ha="center", va="center", fontsize=18, color=MUTED, transform=ax.transAxes)
    ax.text(0.5, 0.22, "Prove it · Write it easily · Run it fast", ha="center", va="center", fontsize=14, color=PRIMARY, transform=ax.transAxes)
    plt.tight_layout()
    save_share(fig, out)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, default=SHARE)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    suites, tp, tf = run_all_suites()
    if not suites:
        print("no suite results — is lic built?", file=sys.stderr)
        return 1

    plot_pass_rate(suites, args.out / "test_suite_pass_rate.png")
    plot_matrix(suites, args.out / "test_suite_matrix.png")
    plot_ci_card(tp, tf, args.out / "ci_summary_card.png")
    print(f"wrote test plots to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
