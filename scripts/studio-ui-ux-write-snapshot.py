#!/usr/bin/env python3
"""Write data/studio-ui-ux-plan-loop/daily-snapshot.json for live canvas refresh."""
from __future__ import annotations

import json
import os
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SNAP = ROOT / "data/studio-ui-ux-plan-loop/daily-snapshot.json"


def main() -> int:
    SNAP.parent.mkdir(parents=True, exist_ok=True)
    plan = ROOT / "docs/superpowers/plans/2026-05-24-studio-ui-ux-plan-loop.md"
    state_path = ROOT / "data/studio-ui-ux-plan-loop/state.json"
    bench_path = ROOT / "data/studio-ui-ux-plan-loop/latest-bench.json"
    ux_path = ROOT / "data/studio-ui-ux-plan-loop/latest-ux-assessment.json"

    todos_total = todos_done = 0
    if plan.is_file():
        text = plan.read_text(encoding="utf-8")
        todos_total = len(re.findall(r"- id: studio-ux-", text))
        todos_done = len(re.findall(r"status: completed", text))

    state = json.loads(state_path.read_text(encoding="utf-8")) if state_path.is_file() else {}
    bench = json.loads(bench_path.read_text(encoding="utf-8")) if bench_path.is_file() else {}
    ux = json.loads(ux_path.read_text(encoding="utf-8")) if ux_path.is_file() else {}
    history = state.get("history", [])[-8:]

    branch = subprocess.run(
        ["git", "-C", str(ROOT), "branch", "--show-current"],
        capture_output=True,
        text=True,
    ).stdout.strip() or "unknown"
    sha = subprocess.run(
        ["git", "-C", str(ROOT), "rev-parse", "--short", "HEAD"],
        capture_output=True,
        text=True,
    ).stdout.strip() or "unknown"

    issue = ""
    ti = ROOT / "data/studio-ui-ux-plan-loop/tracking-issue.txt"
    if ti.is_file():
        issue = ti.read_text(encoding="utf-8").strip()

    day = datetime.now().strftime("%Y-%m-%d")
    snap = {
        "report_date": day,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "tz": os.environ.get("TZ", os.environ.get("STUDIO_UI_UX_TZ", "Europe/Berlin")),
        "branch": branch,
        "head": sha,
        "plan_todos_total": todos_total,
        "plan_todos_completed_yaml": todos_done,
        "state_completed_count": len(state.get("completed_ids", [])),
        "state_completed_ids": state.get("completed_ids", []),
        "state_iterations": state.get("iterations", 0),
        "ux_pass": ux.get("pass", False),
        "ux_avg_score": ux.get("avg_score"),
        "ux_min_score": ux.get("min_score"),
        "ux_dimensions": ux.get("dimensions", {}),
        "bench": bench,
        "history": history,
        "tracking_issue": issue,
        "runner_log": "data/studio-ui-ux-plan-loop/runner.log",
    }
    SNAP.write_text(json.dumps(snap, indent=2) + "\n", encoding="utf-8")
    print(f"studio-ui-ux-write-snapshot: {SNAP}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
