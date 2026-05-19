---
name: ux-audit
description: Run the Li UX audit harness locally (ui-audit.json / ux-audit.json). Use when validating docs, dashboard, or TUI fixtures before swarm testers run.
---

# UX audit (local)

Harness lives in sibling **`../li-cursor-agents/ux-harness/`**.

```bash
cd ../li-cursor-agents
python3 ux-harness/run_audit.py --all --mock
python3 ux-harness/run_audit.py --target lic-docs --mode ui
```

Preflight for briefing: `../benchmarks/scripts/ui-ux-audit.py --mock`

Swarm agents: `docs_ui_tester`, `docs_ux_tester`, `gui_ui_tester`, `gui_ux_tester`, `tui_ui_tester`, `tui_ux_tester` in `li-cursor-agents`.
