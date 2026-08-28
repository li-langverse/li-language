#!/usr/bin/env python3
"""Write per-iteration + rolling sim plan status (validity, perf, memory)."""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
REGISTRY = REPO / "benchmarks" / "competitive" / "algo_registry.json"
CSV_PATH = REPO / "benchmarks" / "results" / "latest.csv"
MEMORY_PATH = REPO / "benchmarks" / "results" / "memory" / "latest_memory.json"
STATE = REPO / "data" / "sim-plan-loop" / "state.json"
REPORT_DIR = REPO / "docs" / "reports" / "sim-plan"
ITER_DIR = REPORT_DIR / "iterations"
DAILY_DIR = REPORT_DIR / "daily"


def git_sha() -> str:
    try:
        return (
            subprocess.check_output(
                ["git", "rev-parse", "--short", "HEAD"], cwd=REPO, text=True
            ).strip()
        )
    except subprocess.CalledProcessError:
        return ""


def registry_stats() -> dict:
    doc = json.loads(REGISTRY.read_text())
    algos = doc.get("algorithms", [])
    impl = sum(1 for a in algos if a.get("implemented_smoke"))
    return {
        "total": len(algos),
        "implemented_smoke": impl,
        "remaining": len(algos) - impl,
        "pct": round(100.0 * impl / len(algos), 1) if algos else 0.0,
    }


def csv_rows_for_benches(benches: set[str]) -> list[dict[str, str]]:
    if not CSV_PATH.is_file() or not benches:
        return []
    rows: list[dict[str, str]] = []
    with CSV_PATH.open(newline="") as f:
        for row in csv.DictReader(f):
            if row.get("benchmark") in benches:
                rows.append(row)
    return rows


def memory_samples() -> list[dict]:
    if not MEMORY_PATH.is_file():
        return []
    return json.loads(MEMORY_PATH.read_text()).get("samples", [])


def load_state() -> dict:
    if STATE.is_file():
        return json.loads(STATE.read_text())
    return {}


def write_iteration_md(
    *,
    package: str,
    benches: list[str],
    stats: dict,
    perf: list[dict],
    mem: list[dict],
) -> Path:
    ITER_DIR.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    path = ITER_DIR / f"{stamp}.md"
    state = load_state()
    last = state.get("history", [{}])[-1] if state.get("history") else {}
    lines = [
        f"# Sim plan iteration {stamp}",
        "",
        f"- **UTC:** {datetime.now(timezone.utc).isoformat()}",
        f"- **git:** `{git_sha()}`",
        f"- **package:** `{package}`",
        f"- **todo:** `{last.get('todo_id', 'n/a')}`",
        f"- **algo progress:** {stats['implemented_smoke']}/{stats['total']} "
        f"({stats['pct']}%)",
        "",
        "## Validity",
        "",
        "- `bench_sim.py --write-summary`",
        "- `./scripts/validate-sim-summary.sh`",
        "- `bench.py --verify-results` (scoped)",
        "",
        "## Performance (scoped CSV)",
        "",
    ]
    if perf:
        lines.append("| benchmark | lang | metric | value | unit |")
        lines.append("|-----------|------|--------|-------|------|")
        for r in perf[:40]:
            lines.append(
                f"| {r.get('benchmark','')} | {r.get('lang','')} | "
                f"{r.get('metric','')} | {r.get('value','')} | {r.get('unit','')} |"
            )
    else:
        lines.append("_No scoped rows in latest.csv yet._")
    lines.extend(["", "## Memory (peak RSS, native)", ""])
    if mem:
        lines.append("| benchmark | peak_rss_kb |")
        lines.append("|-----------|-------------|")
        for s in mem:
            lines.append(f"| {s.get('benchmark','')} | {s.get('peak_rss_kb', 0)} |")
    else:
        lines.append("_No memory samples (build natives first)._")
    lines.extend(
        [
            "",
            "## Benches in scope",
            "",
            ", ".join(f"`{b}`" for b in benches) or "_none_",
            "",
        ]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def write_status_md(stats: dict, *, last_iter: Path | None) -> None:
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    path = REPORT_DIR / "STATUS.md"
    lines = [
        "# Simulation algorithm plan — live status",
        "",
        f"_Updated {datetime.now(timezone.utc).isoformat()}Z_",
        "",
        f"**Registry:** {stats['implemented_smoke']} / {stats['total']} smokes "
        f"({stats['pct']}%) · **remaining:** {stats['remaining']}",
        "",
        f"**Branch:** `{subprocess.check_output(['git', 'branch', '--show-current'], cwd=REPO, text=True).strip()}`",
        f"**HEAD:** `{git_sha()}`",
        "",
    ]
    if last_iter:
        rel = last_iter.relative_to(REPO)
        lines.append(f"**Last iteration report:** [{rel}]({rel})")
    lines.append("")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--package", default="li-sim-scientific")
    args = p.parse_args()

    sys.path.insert(0, str(REPO / "benchmarks" / "harness"))
    from bench_scope import resolve_scope  # noqa: E402

    scope = resolve_scope(packages=[args.package])
    benches = set(scope["benches"])
    stats = registry_stats()
    perf = csv_rows_for_benches(benches)
    mem = memory_samples()
    iter_path = write_iteration_md(
        package=args.package,
        benches=scope["benches"],
        stats=stats,
        perf=perf,
        mem=mem,
    )
    write_status_md(stats, last_iter=iter_path)
    print(iter_path.relative_to(REPO))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
