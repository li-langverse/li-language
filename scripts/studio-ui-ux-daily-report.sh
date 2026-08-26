#!/usr/bin/env bash
# Daily 08:00 operator report — plan progress, UX scores, bench/memory, validity.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV_FILE="${LI_CURSOR_ENV_FILE:-$HOME/Documents/Cursor/.env}"
[[ -f "$ENV_FILE" ]] && set -a && source "$ENV_FILE" && set +a

export TZ="${STUDIO_UI_UX_TZ:-Europe/Berlin}"
DAY="$(date +%Y-%m-%d)"
REPORT_DIR="${ROOT}/docs/reports/studio-ui-ux/daily"
OUT="${REPORT_DIR}/${DAY}.md"
SNAP="${ROOT}/data/studio-ui-ux-plan-loop/daily-snapshot.json"
mkdir -p "$REPORT_DIR" "$(dirname "$SNAP")"

python3 "${ROOT}/scripts/studio-ui-ux-write-snapshot.py"
python3 - "$ROOT" "$DAY" "$SNAP" <<'PY' >"$OUT"
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
day = sys.argv[2]
snap_path = Path(sys.argv[3])
snap = json.loads(snap_path.read_text(encoding="utf-8"))
state = {"completed_ids": snap.get("state_completed_ids", []), "iterations": snap.get("state_iterations", 0)}
completed = state["completed_ids"]
ux = {
    "pass": snap.get("ux_pass", False),
    "avg_score": snap.get("ux_avg_score"),
    "min_score": snap.get("ux_min_score"),
    "dimensions": snap.get("ux_dimensions", {}),
}
bench = snap.get("bench") or {}
history = snap.get("history") or []
branch = snap.get("branch", "unknown")
sha = snap.get("head", "unknown")
issue = snap.get("tracking_issue", "")

lines = [
    f"# Studio UI/UX — daily report {day}",
    "",
    f"_Generated {snap['generated_at']} ({snap['tz']})_",
    "",
    "## Summary",
    "",
    "| Metric | Value |",
    "|--------|-------|",
    f"| Plan todos (state) | **{len(completed)}** completed ids |",
    f"| Iterations run | **{state.get('iterations', 0)}** |",
    f"| UX gate pass | **{ux.get('pass', False)}** |",
    f"| UX avg / min | {ux.get('avg_score', '—')} / {ux.get('min_score', '—')} |",
    f"| Branch | `{branch}` |",
    f"| HEAD | `{sha}` |",
    f"| Tracking issue | {('#' + issue) if issue else '—'} |",
    "",
    "## Last iterations",
    "",
]
if history:
    for row in history:
        lines.append(
            f"- {row.get('at', '')}: `{row.get('todo_id', '')}` "
            f"agent={row.get('agent_exit')} gates={row.get('gates_ok')} ux={row.get('ux_pass')}"
        )
else:
    lines.append("_No history in state.json yet._")

lines.extend(["", "## Bench (latest)", "", "```json", json.dumps(bench, indent=2)[:5000], "```", ""])
lines.extend(["", "## UX dimensions", ""])
dims = ux.get("dimensions") or {}
if dims:
    lines.append("| ID | Score |")
    lines.append("|----|------:|")
    for k, v in sorted(dims.items()):
        sc = v.get("score", v) if isinstance(v, dict) else v
        lines.append(f"| {k} | {sc} |")
else:
    lines.append("_No latest-ux-assessment.json_")

lines.extend([
    "",
    "## Canvas",
    "",
    "Open `canvases/studio-ui-ux-daily-report.canvas.tsx` in Cursor (live refresh via agent-canvases-watch).",
    "",
    "## Gates per iteration",
    "",
    "| Gate | Script |",
    "|------|--------|",
    "| Design system | `studio-ui-ux-generate-design-system.sh` |",
    "| Validity + build | `studio-ui-ux-plan-gates.sh` |",
    "| Perf / memory | `bench-studio-viewport-perf.sh`, `profile-animate-memory.sh` |",
    "| Capture | `studio-ui-ux-capture-progress.sh` |",
    "| Publish | `studio-ui-ux-commit-push.sh` |",
    "",
])
print("\n".join(lines))
PY

cp -f "$OUT" "${REPORT_DIR}/LATEST.md"
echo "studio-ui-ux-daily-report: $OUT"

# Refresh Cursor canvas from snapshot
if [[ -x "${ROOT}/scripts/studio-ui-ux-refresh-canvas.py" ]]; then
  python3 "${ROOT}/scripts/studio-ui-ux-refresh-canvas.py" || true
fi

echo "$(date -Iseconds) daily completed=${SNAP}" >> "${ROOT}/data/studio-ui-ux-plan-loop/daily.log" 2>/dev/null || true
