#!/usr/bin/env python3
"""Regenerate studio-ui-ux-daily-report.canvas.tsx from daily-snapshot.json."""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SNAP = ROOT / "data/studio-ui-ux-plan-loop/daily-snapshot.json"
CANVAS = Path.home() / ".cursor/projects/home-s4il0r-Documents-Cursor/canvases/studio-ui-ux-daily-report.canvas.tsx"


def main() -> int:
    if not SNAP.is_file():
        print("refresh-canvas: no snapshot — run studio-ui-ux-daily-report.sh first")
        return 1
    s = json.loads(SNAP.read_text(encoding="utf-8"))
    dims = s.get("ux_dimensions") or {}
    dim_rows = []
    for k in sorted(dims.keys()):
        v = dims[k]
        sc = v.get("score", v) if isinstance(v, dict) else v
        dim_rows.append(f'          ["{k}", "{sc}"],')
    dim_rows_str = "\n".join(dim_rows) if dim_rows else '          ["—", "—"],'

    hist_rows = []
    for row in s.get("history") or []:
        hist_rows.append(
            f'          ["{row.get("todo_id", "")}", "{row.get("agent_exit", "")}", '
            f'"{row.get("gates_ok", "")}", "{row.get("ux_pass", "")}"],'
        )
    hist_str = "\n".join(hist_rows) if hist_rows else '          ["—", "—", "—", "—"],'

    bench = s.get("bench") or {}
    load_ms = bench.get("load_ms", "—")
    tiers = bench.get("particle_tiers") or []
    tier_rows = []
    for t in tiers[:4]:
        tier_rows.append(
            f'          ["{t.get("id", "")}", "{t.get("particles", "")}", '
            f'"{t.get("fps_target", "")}", "{t.get("status", "")}"],'
        )
    tier_str = "\n".join(tier_rows) if tier_rows else '          ["—", "—", "—", "—"],'

    completed = int(s.get("state_completed_count") or 0)
    total = s.get("plan_todos_total") or 10
    iters = s.get("state_iterations") or 0
    ux_pass = "yes" if s.get("ux_pass") else "no"
    ux_avg = s.get("ux_avg_score") if s.get("ux_avg_score") is not None else "—"
    ux_min = s.get("ux_min_score") if s.get("ux_min_score") is not None else "—"

    content = f'''import {{
  BarChart,
  Callout,
  Divider,
  Grid,
  H1,
  H2,
  Stack,
  Stat,
  Table,
  Text,
  UsageBar,
}} from "cursor/canvas";

const REPORT_DATE = "{s.get("report_date", "")}";
const GENERATED_AT = "{s.get("generated_at", "")} ({s.get("tz", "")})";
const BRANCH = "{s.get("branch", "")}";
const HEAD = "{s.get("head", "")}";
const COMPLETED = {completed};
const TOTAL = {total};
const ITERATIONS = {iters};
const UX_PASS = "{ux_pass}";
const UX_AVG = "{ux_avg}";
const UX_MIN = "{ux_min}";
const LOAD_MS = "{load_ms}";

const UX_DIM_ROWS: string[][] = [
{dim_rows_str}
];

const HIST_ROWS: string[][] = [
{hist_str}
];

const TIER_ROWS: string[][] = [
{tier_str}
];

export default function StudioUiUxDailyReport() {{
  const pct = TOTAL > 0 ? Math.round((COMPLETED / TOTAL) * 1000) / 10 : 0;
  const dimScores = UX_DIM_ROWS.map((r) => Number(r[1]) || 0);
  const dimLabels = UX_DIM_ROWS.map((r) => r[0]);

  return (
    <Stack gap={20}>
      <Stack gap={4}>
        <H1>Studio UI/UX — daily report</H1>
        <Text tone="secondary" size="small">
          {{REPORT_DATE}} · generated {{GENERATED_AT}}
        </Text>
        <Text tone="secondary" size="small">
          Source: data/studio-ui-ux-plan-loop/daily-snapshot.json · live refresh every 15s
          via scripts/agent-canvases-watch.sh
        </Text>
      </Stack>

      <Grid columns={4} gap={12}>
        <Stat value={{`${{COMPLETED}}/${{TOTAL}}`}} label="Plan todos done" />
        <Stat value={{`${{ITERATIONS}}`}} label="Iterations" tone="success" />
        <Stat value={{UX_PASS}} label="UX gate pass" tone={{UX_PASS === "yes" ? "success" : "warning"}} />
        <Stat value={{`${{LOAD_MS}}`}} label="Load proxy (ms)" />
      </Grid>

      <UsageBar
        total={{TOTAL}}
        topLeftLabel={{`${{pct}}% plan todos in state`}}
        topRightLabel={{`UX avg ${{UX_AVG}} · min ${{UX_MIN}}`}}
        segments={{[{{ id: "done", value: COMPLETED, color: "green" }}]}}
      />

      <Grid columns={2} gap={16}>
        <Stack gap={8}>
          <H2>UX dimension scores</H2>
          <BarChart
            categories={{dimLabels}}
            series={{[{{ name: "score_0_3", data: dimScores, tone: "success" }}]}}
            horizontal
            height={{220}}
            valueSuffix=""
          />
          <Text tone="secondary" size="small">
            Target: avg ≥ 2.0, min ≥ 1.5 for UX gate pass
          </Text>
        </Stack>
        <Stack gap={8}>
          <H2>MD particle tiers</H2>
          <Table
            headers={{["Tier", "Particles", "FPS target", "Status"]}}
            rows={{TIER_ROWS}}
          />
        </Stack>
      </Grid>

      <Divider />

      <H2>Autonomous runner</H2>
      <Callout tone="info" title="studio_ui_ux_builder until UX pass">
        <Stack gap={6}>
          <Text>
            Branch {{BRANCH}} ({{HEAD}}). Each iteration: design system, gates,
            bench, capture, commit+push. Log: data/studio-ui-ux-plan-loop/runner.log
          </Text>
        </Stack>
      </Callout>

      <H2>Last iterations</H2>
      <Table
        headers={{["Todo", "Agent exit", "Gates", "UX pass"]}}
        rows={{HIST_ROWS}}
      />

      <Divider />

      <H2>UX rubric snapshot</H2>
      <Table headers={{["Dimension", "Score"]}} rows={{UX_DIM_ROWS}} />

      <Text tone="secondary" size="small">
        Tracking issue: {s.get("tracking_issue") or "auto-created on first capture"}
      </Text>
    </Stack>
  );
}}
'''
    CANVAS.parent.mkdir(parents=True, exist_ok=True)
    CANVAS.write_text(content, encoding="utf-8")
    print(f"refresh-canvas: {CANVAS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
