#!/usr/bin/env python3
"""Write per-iteration report under docs/reports/studio-ui-ux/iterations/."""
from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / "data/studio-ui-ux-plan-loop/state.json"
BENCH = ROOT / "data/studio-ui-ux-plan-loop/latest-bench.json"
UX = ROOT / "data/studio-ui-ux-plan-loop/latest-ux-assessment.json"
OUT_DIR = ROOT / "docs/reports/studio-ui-ux/iterations"


def main() -> int:
    todo_id = sys.argv[1] if len(sys.argv) > 1 else "unknown"
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    out = OUT_DIR / f"{stamp}-{todo_id}.md"

    state = json.loads(STATE.read_text()) if STATE.is_file() else {}
    bench = json.loads(BENCH.read_text()) if BENCH.is_file() else {}
    ux = json.loads(UX.read_text()) if UX.is_file() else {}

    lines = [
        f"# Studio UI/UX iteration — `{todo_id}`",
        "",
        f"_UTC {datetime.now(timezone.utc).isoformat()}_",
        "",
        "## UX assessment",
        "",
    ]
    if ux:
        lines.append(f"- **pass:** {ux.get('pass', False)}")
        lines.append(f"- **avg_score:** {ux.get('avg_score', 'n/a')}")
        lines.append(f"- **min_score:** {ux.get('min_score', 'n/a')}")
        dims = ux.get("dimensions") or {}
        if dims:
            lines.append("")
            lines.append("| ID | Score | Note |")
            lines.append("|----|------:|------|")
            for k, v in sorted(dims.items()):
                if isinstance(v, dict):
                    lines.append(f"| {k} | {v.get('score', '?')} | {v.get('note', '')} |")
    else:
        lines.append("_No latest-ux-assessment.json — agent must write this file._")

    lines.extend(["", "## Bench", "", "```json", json.dumps(bench, indent=2)[:6000], "```", ""])
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
